#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>
#include <graphene/chain/database.hpp>

#include <iostream>

namespace graphene { namespace chain {

   /**
    * @class cheque_object
    * @ingroup object
    * @ingroup protocol
    *
    * This object contains information and logic of cheque
    */
   class cheque_object: public db::abstract_object<cheque_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = cheque_object_type;

      cheque_id_type            get_id() const { return id; }
      const fc::time_point_sec& get_expiration_datetime() const { return datetime_expiration; }
      const account_id_type&    get_drawer() const { return drawer; }
      const cheque_status&      get_cheque_status() const { return status; }
      void                      allocate_payees(uint32_t payees_count);
      void                      process_payee(account_id_type payee, database& db);

      asset get_remaining_amount() const {
         return asset(amount_remaining, asset_id);
      }

      // cheque activation code
      std::string code;

      // cheque object: date and time
      fc::time_point_sec datetime_creation;

      // cheque expiration date and time
      fc::time_point_sec datetime_expiration;

      // cheque date and time, when it was used
      fc::time_point_sec datetime_used;

      // account which has created cheque
      account_id_type drawer;

      // amount of subcheque (payment for each user)
      share_type amount_payee;

      // remaining amount after payments
      share_type amount_remaining;

      // receipt asset id
      asset_id_type asset_id;

      // cheque status (send / receive)
      cheque_status status = cheque_status::cheque_new;

      // all payees are initialize on creation time in
      struct payee_item
      {
         // account which has used part of cheque
         account_id_type payee;

         // cheque date and time, when it was used
         fc::time_point_sec datetime_used;

         // subcheque status
         cheque_status status = cheque_status::cheque_new;
      };

      std::vector<payee_item> payees;

   }; // cheque_object

   struct by_drawer;
   struct by_code;
   struct by_datetime_exp;
   struct by_datetime_creation;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
   cheque_object,
   indexed_by<
         ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
         ordered_unique<tag<by_code>, member<cheque_object, std::string, &cheque_object::code>>,
         ordered_non_unique<tag<by_drawer>, member<cheque_object, account_id_type, &cheque_object::drawer>>,
         ordered_non_unique<tag<by_datetime_creation>, member<cheque_object, fc::time_point_sec, &cheque_object::datetime_creation>>,
         ordered_non_unique<tag<by_datetime_exp>, member<cheque_object, fc::time_point_sec, &cheque_object::datetime_expiration>>
      >
   > cheque_object_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<cheque_object, cheque_object_index_type> cheque_index;

}}

FC_REFLECT( graphene::chain::cheque_object::payee_item,
            (payee)
            (datetime_used)
            (status));

FC_REFLECT_DERIVED( graphene::chain::cheque_object,
                    (graphene::db::object),
                    (code)
                    (datetime_creation)
                    (datetime_expiration)
                    (datetime_used)
                    (drawer)
                    (amount_payee)
                    (amount_remaining)
                    (asset_id)
                    (status)
                    (payees)
                  );

