#pragma once

#include <graphene/protocol/referral_classes.hpp>
#include <graphene/protocol/ext.hpp>
#include "../../chain/include/graphene/chain/types.hpp"

namespace graphene { namespace protocol {

   struct fund_options
   {
      struct payment_rate
      {
         // deposit period, days
         uint32_t period = 0;
         // percent for whole period
         uint32_t percent = 0;
      };

      struct fund_rate
      {
         share_type amount = 0;
         uint32_t day_percent = 0;
      };

      void validate() const;

      std::string description;
      uint16_t    period = 0;
      share_type  min_deposit = 0;
      uint16_t    rates_reduction_per_month = 0;

      std::vector<fund_rate>    fund_rates;
      std::vector<payment_rate> payment_rates;

   };

   struct fund_ext_info
   {
      share_type      max_limit_deposits_amount;
      bool            owner_monthly_payments_enabled = false;
      account_id_type mlm_account;
      bool            mlm_monthly_payments_enabled = false;
      uint32_t        mlm_daily_percent = 0;
   };
   typedef static_variant<void_t, fund_ext_info>::flat_set_type fund_ext_type;

   /**
    * @ingroup operations
    */
   struct fund_create_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      std::string     name;
      account_id_type owner;
      asset_id_type   asset_id;
      fund_options    options;

      fund_ext_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_update_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    id; // fund id
      fund_options    options;

      fund_ext_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_refill_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    id; // fund id
      share_type      amount;

      extensions_type extensions;
      account_id_type fee_payer() const { return from_account; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_deposit_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    fund_id;
      share_type      amount;
      uint32_t        period;

      extensions_type extensions;
      account_id_type fee_payer() const { return from_account; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_withdrawal_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type issuer;

      fund_id_type       fund_id;
      asset              asset_to_issue; // amount
      account_id_type    issue_to_account;
      fc::time_point_sec datetime; // block datetime

      extensions_type extensions;
      account_id_type fee_payer() const { return issuer; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_payment_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type issuer;

      fund_id_type    fund_id;
      optional<graphene::chain::fund_deposit_id_type> deposit_id;

      asset           asset_to_issue; // amount to user
      account_id_type issue_to_account;

      extensions_type extensions;
      account_id_type fee_payer() const { return issuer; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_set_enable_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      fund_id_type    id; // fund id
      bool            enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_deposit_set_enable_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      graphene::chain::fund_deposit_id_type deposit_id;
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_remove_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_id_type id; // fund id

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_change_payment_scheme_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_id_type id; // fund id

      // see fund_object.hpp
      uint8_t payment_scheme = 0;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct enable_autorenewal_deposits_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      account_id_type account_id;
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return account_id; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct deposit_renewal_operation: public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset            fee;

      account_id_type    account_id;
      graphene::chain::fund_deposit_id_type deposit_id;
      uint32_t           percent = 0;
      fc::time_point_sec datetime_end;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& k) const { return 0; };

   };

   // deprecated
   struct fund_deposit_update_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      struct ext
      {
         optional<void_t> null_ext;
         optional<bool>               apply_extension_flags_only;
         optional<account_id_type>    account_id;
         optional<fc::time_point_sec> datetime_end;
      };

      asset fee;

      graphene::chain::fund_deposit_id_type deposit_id;

      uint32_t percent = 0;
      bool     reset = false;

      extension<ext> extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   struct fund_dep_upd_ext_info
   {
      bool               apply_extension_flags_only = false;
      account_id_type    account_id;
      fc::time_point_sec datetime_end;
   };
   typedef static_variant<void_t, fund_dep_upd_ext_info>::flat_set_type fund_dep_upd_ext_type;

   /**
    * @ingroup operations
    */
   struct fund_deposit_update2_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      graphene::chain::fund_deposit_id_type deposit_id;

      uint32_t percent = 0;
      bool     reset = false;

      fund_dep_upd_ext_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_deposit_reduce_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      graphene::chain::fund_deposit_id_type deposit_id;
      share_type amount;

      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct fund_deposit_acceptance_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_id_type fund_id;
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

   /**
    * @ingroup operations
    */
   struct hard_enable_autorenewal_deposits_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      vector<account_id_type> accounts;
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::fund_options::payment_rate,
            (period)(percent) )

FC_REFLECT( graphene::protocol::fund_options::fund_rate,
            (amount)(day_percent) )

FC_REFLECT( graphene::protocol::fund_options,
            (description)(period)(min_deposit)(rates_reduction_per_month)(fund_rates)(payment_rates) )

FC_REFLECT( graphene::protocol::fund_ext_info,
            (max_limit_deposits_amount)
            (owner_monthly_payments_enabled)
            (mlm_account)
            (mlm_monthly_payments_enabled)
            (mlm_daily_percent) )
FC_REFLECT( graphene::protocol::fund_dep_upd_ext_info, (apply_extension_flags_only)(account_id)(datetime_end) )

FC_REFLECT( graphene::protocol::fund_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_refill_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_deposit_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_withdrawal_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_payment_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_set_enable_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_deposit_set_enable_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_remove_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_change_payment_scheme_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::enable_autorenewal_deposits_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::deposit_renewal_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::fund_deposit_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_deposit_update2_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_deposit_reduce_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::fund_deposit_acceptance_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::hard_enable_autorenewal_deposits_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::fund_refill_operation,
            (fee)(from_account)(id)(amount)(extensions) )
FC_REFLECT( graphene::protocol::fund_deposit_operation,
            (fee)(from_account)(fund_id)(amount)(period)(extensions) )
FC_REFLECT( graphene::protocol::fund_withdrawal_operation,
            (fee)(issuer)(fund_id)(asset_to_issue)(issue_to_account)(datetime)(extensions) )
FC_REFLECT( graphene::protocol::fund_payment_operation,
            (fee)(issuer)(fund_id)(deposit_id)(asset_to_issue)(issue_to_account)(extensions) )
FC_REFLECT( graphene::protocol::fund_set_enable_operation,
            (fee)(id)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::fund_deposit_set_enable_operation,
            (fee)(deposit_id)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::fund_remove_operation,
            (fee)(id)(extensions) )
FC_REFLECT( graphene::protocol::fund_change_payment_scheme_operation,
            (fee)(id)(payment_scheme)(extensions) )
FC_REFLECT( graphene::protocol::enable_autorenewal_deposits_operation,
            (fee)(account_id)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::deposit_renewal_operation, (fee)(account_id)(deposit_id)(percent)(datetime_end) )
FC_REFLECT( graphene::protocol::fund_deposit_reduce_operation, (deposit_id)(amount)(extensions) )
FC_REFLECT( graphene::protocol::fund_deposit_acceptance_operation,
            (fee)(fund_id)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::hard_enable_autorenewal_deposits_operation,
            (fee)(accounts)(enabled)(extensions) )

FC_REFLECT( graphene::protocol::fund_create_operation, (fee)(name)(owner)(asset_id)(options)(extensions) )
FC_REFLECT( graphene::protocol::fund_update_operation, (fee)(from_account)(id)(options)(extensions) )

FC_REFLECT( graphene::protocol::fund_deposit_update_operation::ext, (null_ext)(apply_extension_flags_only)(account_id)(datetime_end) )
FC_REFLECT_TYPENAME(graphene::protocol::extension<graphene::protocol::fund_deposit_update_operation::ext>)
FC_REFLECT( graphene::protocol::fund_deposit_update_operation, (fee)(deposit_id)(percent)(reset)(extensions) )
FC_REFLECT( graphene::protocol::fund_deposit_update2_operation, (fee)(deposit_id)(percent)(reset)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_ext_info )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_dep_upd_ext_info )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_refill_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_withdrawal_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_payment_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_set_enable_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_set_enable_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_remove_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_change_payment_scheme_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::enable_autorenewal_deposits_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::deposit_renewal_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_update2_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_reduce_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_acceptance_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::hard_enable_autorenewal_deposits_operation::fee_parameters_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_create_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_update_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_refill_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_withdrawal_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_payment_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_set_enable_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_set_enable_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_remove_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_change_payment_scheme_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::enable_autorenewal_deposits_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::deposit_renewal_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_update_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_update2_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_reduce_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fund_deposit_acceptance_operation)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::hard_enable_autorenewal_deposits_operation)