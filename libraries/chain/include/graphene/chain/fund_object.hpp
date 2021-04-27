#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>
#include <graphene/chain/database.hpp>

#include <iostream>

namespace graphene { namespace chain {

   enum fund_payment_scheme
   {
      residual,
      fixed
   };

   /**
    * @class fund_history_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains historical data about a fund.
    */
   class fund_history_object: public db::abstract_object<fund_history_object>
   {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_fund_history_object_type;

      fund_id_type owner;

      // new item will be added on each maintenance time
      struct history_item
      {
         fc::time_point create_datetime;
         share_type daily_payments_without_owner;
         share_type daily_payments_total;
         share_type daily_payments_owner;
      };

      std::vector<history_item> items;

   }; // fund_history_object

   /////////////////////////////////////////////////////////////////////////////////////////////////

   /**
    * @class fund_statistics_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about a fund.
    */
   class fund_statistics_object: public db::abstract_object<fund_statistics_object>
   {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_fund_statistics_object_type;

      fund_id_type owner;

      /**
       * Keep the most recent operation as a root pointer to a linked list of the transaction history.
       */
      fund_transaction_history_id_type most_recent_op;
      uint32_t total_ops = 0;

      // count of all deposits
      int64_t deposit_count = 0;

   }; // fund_statistics_object

   /////////////////////////////////////////////////////////////////////////////////////////////////

   /**
    *  @brief a node in a linked list of operation_history_objects
    *  @ingroup implementation
    *  @ingroup object
    *
    *  Contains information of fund's operation
    */
   class fund_transaction_history_object: public db::abstract_object<fund_transaction_history_object>
   {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id  = impl_fund_transaction_history_object_type;

      fund_id_type                     fund;
      operation_history_id_type        operation_id;
      uint32_t                         sequence = 0; // the operation position within the given fund
      fund_transaction_history_id_type next;
      fc::time_point_sec               block_time;

   }; // fund_transaction_history_object

   struct by_time;
   struct by_seq;
   struct by_op;

   typedef multi_index_container<
      fund_transaction_history_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
         ordered_non_unique<tag<by_time>, member<fund_transaction_history_object, fc::time_point_sec, &fund_transaction_history_object::block_time>>,
         ordered_unique<tag<by_seq>,
            composite_key<fund_transaction_history_object,
               member<fund_transaction_history_object, fund_id_type, &fund_transaction_history_object::fund>,
               member<fund_transaction_history_object, uint32_t, &fund_transaction_history_object::sequence>
            >
         >,
         ordered_unique<tag<by_op>,
            composite_key<fund_transaction_history_object,
               member<fund_transaction_history_object, fund_id_type, &fund_transaction_history_object::fund>,
               member<fund_transaction_history_object, operation_history_id_type, &fund_transaction_history_object::operation_id>
            >
         >
      >
   > fund_transaction_history_multi_index_type;

   typedef generic_index<fund_transaction_history_object, fund_transaction_history_multi_index_type> fund_transaction_history_index;

   /////////////////////////////////////////////////////////////////////////////////////////////////

   /**
    * @class fund_deposit_object
    * @ingroup object
    * @ingroup implementation
    *
    * This is a deposit which can be applied to fund
    */
   class fund_deposit_object: public db::abstract_object<fund_deposit_object>
   {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id  = impl_fund_deposit_object_type;

      fund_deposit_id_type get_id() const { return id; }

      fund_id_type       fund_id;        // fund to which the deposit belongs
      account_id_type    account_id;
      asset              amount;
      bool               enabled = true;
      bool               finished = false;
      fc::time_point_sec datetime_begin; // datetime of creation
      fc::time_point_sec datetime_end;   // datetime of deposit's finishing
      fc::time_point_sec prev_maintenance_time_on_creation; // previous maintenance time at deposit's creation moment

      // lifetime of user's deposit (for paying percents), in days
      uint32_t           period = 30;
      uint32_t           percent = 0;
      share_type         daily_payment;
      bool               manual_percent_enabled = false;
      // for auto-disabling of payments for this deposit
      bool               can_use_percent = true;

   }; // fund_deposit_object

   struct by_account_id;
   struct by_fund_id;
   struct by_period;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      fund_deposit_object,
         indexed_by<
            ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
            ordered_non_unique<tag<by_account_id>, member<fund_deposit_object, account_id_type, &fund_deposit_object::account_id>>,
            ordered_non_unique<tag<by_fund_id>, member<fund_deposit_object, fund_id_type, &fund_deposit_object::fund_id>>,
            ordered_non_unique<tag<by_period>, member<fund_deposit_object, uint32_t, &fund_deposit_object::period>>
         >
   > fund_deposit_object_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<fund_deposit_object, fund_deposit_object_index_type> fund_deposit_index;

   /////////////////////////////////////////////////////////////////////////////////////////////////

   /**
    * @class fund_object
    * @ingroup object
    * @ingroup protocol
    *
    * This object contains information and logic of fund
    */
   class fund_object: public db::abstract_object<fund_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = fund_object_type;

      fund_id_type get_id() const { return id; }

      // percent according to appropriate 'fund_rate' item
      double get_rate_percent(const fund_options::fund_rate& f_item, const database& db) const;

      optional<fund_options::fund_rate>
      get_max_fund_rate(const share_type& amnt) const;

      optional<fund_options::payment_rate>
      get_payment_rate(uint32_t period) const;

      // make fund payments
      void process(database& db) const;
      // make fund owner withdrawal
      void finish(database& db) const;

      std::string        name;
      std::string        description;
      account_id_type    owner;
      asset_id_type      asset_id;       // fund asset id
      share_type         balance_before_hf640;
      share_type         balance;        // sum of all refill operations and user deposits;
      share_type         owner_balance;  // sum of all owner's refills
      share_type         max_limit_deposits_amount; // maximum amount of deposits sum
      bool               owner_monthly_payments_enabled = false; // if enabled then make payments to fund owner once per 30 days
      fc::time_point_sec owner_monthly_payments_next_datetime;
      share_type         owner_monthly_payments_amount;
      account_id_type    mlm_account; // account for making mlm-payment
      bool               mlm_monthly_payments_enabled = false; // if enabled then make payments to mlm-account once per 30 days
      fc::time_point_sec mlm_monthly_payments_next_datetime;
      uint32_t           mlm_daily_percent = 0; // daily percent for MLM-payments
      share_type         mlm_monthly_payments_amount;
      bool               enabled = true; // don't calculate fund's profit if 'false'
      fc::time_point_sec datetime_begin; // datetime of creation & start
      fc::time_point_sec datetime_end;   // datetime of fund's finishing
      fc::time_point_sec prev_maintenance_time_on_creation; // previous maintenance time at fund's creation moment
      bool deposit_acceptance_enabled = true;

      // each month fund_rate::day_percent decreasing by this value
      uint16_t rates_reduction_per_month = 0;

      // fund existing period, in days
      uint32_t period = 0;

      // minimal deposit to the current fund
      share_type min_deposit;

      // The reference implementation records the fund's statistics in a separate object. This field contains the
      // ID of that object.
      fund_statistics_id_type statistics_id;

      // ID of history object
      fund_history_id_type history_id;

      // current payment scheme
      fund_payment_scheme payment_scheme = fund_payment_scheme::residual;

      /**
       * @brief Payment percents to user according to its
       * deposit (on each maintenance interval).
       * Should contain value of user's deposit 'period'.
       */
      std::vector<fund_options::payment_rate> payment_rates;

      /**
       * @brief Payments to fund owner
       */
      std::vector<fund_options::fund_rate> fund_rates;

   }; // fund_object

   struct by_name;
   struct by_owner;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      fund_object,
         indexed_by<
            ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
            ordered_unique<tag<by_name>, member<fund_object, std::string, &fund_object::name>>,
            ordered_unique<tag<by_owner>, member<fund_object, account_id_type, &fund_object::owner>>
         >
   > fund_object_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<fund_object, fund_object_index_type> fund_index;

}}

FC_REFLECT_ENUM( graphene::chain::fund_payment_scheme,
                 (residual)(fixed))

FC_REFLECT( graphene::chain::fund_history_object::history_item,
            (create_datetime)
            (daily_payments_without_owner)
            (daily_payments_total)
            (daily_payments_owner) )

MAP_OBJECT_ID_TO_TYPE(graphene::chain::fund_history_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::fund_statistics_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::fund_transaction_history_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::fund_deposit_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::fund_object)

FC_REFLECT_TYPENAME( graphene::chain::fund_history_object )
FC_REFLECT_TYPENAME( graphene::chain::fund_statistics_object )
FC_REFLECT_TYPENAME( graphene::chain::fund_transaction_history_object )
FC_REFLECT_TYPENAME( graphene::chain::fund_deposit_object )
FC_REFLECT_TYPENAME( graphene::chain::fund_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fund_history_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fund_statistics_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fund_transaction_history_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fund_deposit_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::fund_object )