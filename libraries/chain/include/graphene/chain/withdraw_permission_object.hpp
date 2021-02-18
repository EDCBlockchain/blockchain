// see LICENSE.txt

#pragma once
#include <graphene/protocol/authority.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

  /**
   * @class withdraw_permission_object
   * @brief Grants another account authority to withdraw a limited amount of funds per interval
   *
   * The primary purpose of this object is to enable recurring payments on the blockchain. An account which wishes to
   * process a recurring payment may use a @ref withdraw_permission_claim_operation to reference an object of this type
   * and withdraw up to @ref withdrawal_limit from @ref withdraw_from_account. Only @ref authorized_account may do
   * this. Any number of withdrawals may be made so long as the total amount withdrawn per period does not exceed the
   * limit for any given period.
   */
  class withdraw_permission_object : public graphene::db::abstract_object<withdraw_permission_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = withdraw_permission_object_type;

        /// The account authorizing @ref authorized_account to withdraw from it
        account_id_type    withdraw_from_account;
        /// The account authorized to make withdrawals from @ref withdraw_from_account
        account_id_type    authorized_account;
        /// The maximum amount which may be withdrawn per period. All withdrawals must be of this asset type
        asset              withdrawal_limit;
        /// The duration of a withdrawal period in seconds
        uint32_t           withdrawal_period_sec = 0;
        /// The beginning of the next withdrawal period
        time_point_sec     period_start_time;
        /// The time at which this withdraw permission expires
        time_point_sec     expiration;

        /// tracks the total amount
        share_type         claimed_this_period;
        /// True if the permission may still be claimed for this period; false if it has already been used
        asset              available_this_period( fc::time_point_sec current_time )const
        {
           if( current_time >= period_start_time + withdrawal_period_sec )
              return withdrawal_limit;
           return asset(
              ( withdrawal_limit.amount > claimed_this_period )
              ? withdrawal_limit.amount - claimed_this_period
              : 0, withdrawal_limit.asset_id );
        }
   };

   struct by_from;
   struct by_authorized;
   struct by_expiration;

   typedef multi_index_container<
      withdraw_permission_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_from>,
            composite_key< withdraw_permission_object,
               member<withdraw_permission_object, account_id_type, &withdraw_permission_object::withdraw_from_account>,
               member< object, object_id_type, &object::id >
            >
         >,
         ordered_unique< tag<by_authorized>,
            composite_key< withdraw_permission_object,
               member<withdraw_permission_object, account_id_type, &withdraw_permission_object::authorized_account>,
               member< object, object_id_type, &object::id >
            >
         >,
         ordered_unique< tag<by_expiration>,
            composite_key< withdraw_permission_object,
               member<withdraw_permission_object, time_point_sec, &withdraw_permission_object::expiration>,
               member< object, object_id_type, &object::id >
            >
         >
      >
   > withdraw_permission_object_multi_index_type;

   typedef generic_index<withdraw_permission_object, withdraw_permission_object_multi_index_type> withdraw_permission_index;


} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::withdraw_permission_object)

FC_REFLECT_TYPENAME( graphene::chain::withdraw_permission_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::withdraw_permission_object )
