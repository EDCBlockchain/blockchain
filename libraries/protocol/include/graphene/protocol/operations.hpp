// see LICENSE.txt

#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/referral_classes.hpp>
#include <graphene/protocol/account.hpp>
#include <graphene/protocol/assert.hpp>
#include <graphene/protocol/asset_ops.hpp>
#include <graphene/protocol/fund_ops.hpp>
#include <graphene/protocol/cheque_ops.hpp>
#include <graphene/protocol/settings_ops.hpp>
#include <graphene/protocol/balance.hpp>
#include <graphene/protocol/custom.hpp>
#include <graphene/protocol/committee_member.hpp>
#include <graphene/protocol/confidential.hpp>
#include <graphene/protocol/fba.hpp>
#include <graphene/protocol/market.hpp>
#include <graphene/protocol/proposal.hpp>
#include <graphene/protocol/transfer.hpp>
#include <graphene/protocol/vesting.hpp>
#include <graphene/protocol/withdraw_permission.hpp>
#include <graphene/protocol/witness.hpp>
#include <graphene/protocol/worker.hpp>
#include <graphene/protocol/witnesses_info_ops.hpp>

namespace graphene { namespace protocol {

   /**
    * @ingroup operations
    *
    * Defines the set of valid operations as a discriminated union type.
    */
   typedef fc::static_variant<
            /*  0 */ transfer_operation,
            /*  1 */ limit_order_create_operation,
            /*  2 */ limit_order_cancel_operation,
            /*  3 */ call_order_update_operation,
            /*  4 */ fill_order_operation,
            /*  5 */ account_create_operation,
            /*  6 */ account_update_operation,
            /*  7 */ account_whitelist_operation,
            /*  8 */ account_upgrade_operation,
            /*  9 */ account_transfer_operation,
            /* 10 */ asset_create_operation,
            /* 11 */ asset_update_operation,
            /* 12 */ asset_update_bitasset_operation,
            /* 13 */ asset_update_feed_producers_operation,
            /* 14 */ asset_issue_operation,
            /* 15 */ asset_reserve_operation,
            /* 16 */ asset_fund_fee_pool_operation,
            /* 17 */ asset_settle_operation,
            /* 18 */ asset_global_settle_operation,
            /* 19 */ asset_publish_feed_operation,
            /* 20 */ witness_create_operation,
            /* 21 */ witness_update_operation,
            /* 22 */ proposal_create_operation,
            /* 23 */ proposal_update_operation,
            /* 24 */ proposal_delete_operation,
            /* 25 */ withdraw_permission_create_operation,
            /* 26 */ withdraw_permission_update_operation,
            /* 27 */ withdraw_permission_claim_operation,
            /* 28 */ withdraw_permission_delete_operation,
            /* 29 */ committee_member_create_operation,
            /* 30 */ committee_member_update_operation,
            /* 31 */ committee_member_update_global_parameters_operation,
            /* 32 */ vesting_balance_create_operation,
            /* 33 */ vesting_balance_withdraw_operation,
            /* 34 */ worker_create_operation,
            /* 35 */ custom_operation,
            /* 36 */ assert_operation,
            /* 37 */ balance_claim_operation,
            /* 38 */ override_transfer_operation,
            /* 39 */ transfer_to_blind_operation,
            /* 40 */ blind_transfer_operation,
            /* 41 */ transfer_from_blind_operation,
            /* 42 */ asset_settle_cancel_operation,
            /* 43 */ asset_claim_fees_operation,
            /* 44 */ fba_distribute_operation,
            /* 45 */ bonus_operation,
            /* 46 */ daily_issue_operation,
            /* 47 */ referral_issue_operation,
            /* 48 */ edc_asset_fund_fee_pool_operation,
            /* 49 */ account_restrict_operation,        // restrict account money_in, money_out, bonuses
            /* 50 */ account_allow_registrar_operation, // account can create other accounts
            /* 51 */ set_online_time_operation,
            /* 52 */ set_verification_is_required_operation,
            /* 53 */ allow_create_asset_operation,
            /* 54 */ add_address_operation,
            /**
             * we can't edit old 'asset_update_operation' because of
             * backward compatibility
             */
            /* 55 */ asset_update2_operation,
            /* 56 */ fund_create_operation,
            /* 57 */ fund_update_operation,
            /* 58 */ fund_refill_operation,
            /* 59 */ fund_deposit_operation,
            /* 60 */ fund_withdrawal_operation,
            /* 61 */ fund_payment_operation,
            /* 62 */ fund_set_enable_operation,
            /* 63 */ fund_deposit_set_enable_operation,
            /* 64 */ fund_remove_operation,
            /* 65 */ allow_create_addresses_operation,
            /* 66 */ set_burning_mode_operation,
            /* 67 */ assets_update_fee_payer_operation,
            /* 68 */ asset_update_exchange_rate_operation,
            /* 69 */ fund_change_payment_scheme_operation,
            /* 70 */ enable_autorenewal_deposits_operation,
            /* 71 */ update_blind_transfer2_settings_operation,
            /* 72 */ blind_transfer2_operation,
            /* 73 */ deposit_renewal_operation,  // extends deposit time
            /* 74 */ set_market_operation,
            /* 75 */ cheque_create_operation,
            /* 76 */ cheque_use_operation,
            /* 77 */ cheque_reverse_operation,
            /* 78 */ create_market_address_operation,
            /* 79 */ account_update_authorities_operation,
            /* 80 */ update_settings_operation,
            /* 81 */ account_edc_limit_daily_volume_operation,
            /* 82 */ fund_deposit_update_operation,
            /* 83 */ fund_deposit_reduce_operation,
            /* 84 */ fund_deposit_update2_operation,
            /* 85 */ denominate_operation,
            /* 86 */ set_witness_exception_operation,
            /* 87 */ update_referral_settings_operation,
            /* 88 */ update_accounts_referrer_operation,
            /* 89 */ enable_account_referral_payments_operation,
            /* 90 */ fund_deposit_acceptance_operation,
            /* 91 */ hard_enable_autorenewal_deposits_operation
         > operation;

   /// @} // operations group

   /**
    *  Appends required authorites to the result vector.  The authorities appended are not the
    *  same as those returned by get_required_auth 
    *
    *  @return a set of required authorities for @ref op
    */
   void operation_get_required_authorities( const operation& op, 
                                            flat_set<account_id_type>& active,
                                            flat_set<account_id_type>& owner,
                                            vector<authority>&  other );

   void operation_validate( const operation& op );

   /**
    *  @brief necessary to support nested operations inside the proposal_create_operation
    */
   struct op_wrapper
   {
      public:
         op_wrapper(const operation& op = operation()):op(op){}
         operation op;
   };

} } // graphene::protocol

FC_REFLECT_TYPENAME( graphene::protocol::operation )
FC_REFLECT( graphene::protocol::op_wrapper, (op) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::op_wrapper )