/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/referral_classes.hpp>
#include <graphene/chain/protocol/account.hpp>
#include <graphene/chain/protocol/assert.hpp>
#include <graphene/chain/protocol/asset_ops.hpp>
#include <graphene/chain/protocol/fund_ops.hpp>
#include <graphene/chain/protocol/balance.hpp>
#include <graphene/chain/protocol/custom.hpp>
#include <graphene/chain/protocol/committee_member.hpp>
#include <graphene/chain/protocol/confidential.hpp>
#include <graphene/chain/protocol/fba.hpp>
#include <graphene/chain/protocol/market.hpp>
#include <graphene/chain/protocol/proposal.hpp>
#include <graphene/chain/protocol/transfer.hpp>
#include <graphene/chain/protocol/vesting.hpp>
#include <graphene/chain/protocol/withdraw_permission.hpp>
#include <graphene/chain/protocol/witness.hpp>
#include <graphene/chain/protocol/worker.hpp>

namespace graphene { namespace chain {

   /**
    * @ingroup operations
    *
    * Defines the set of valid operations as a discriminated union type.
    */
   typedef fc::static_variant<
            transfer_operation,            // [idx: 0]
            limit_order_create_operation,
            limit_order_cancel_operation,
            call_order_update_operation,   // [idx: 3]
            fill_order_operation,           // VIRTUAL
            account_create_operation,
            account_update_operation,      // [idx: 6]
            account_whitelist_operation,
            account_upgrade_operation,
            account_transfer_operation,    // [idx: 9]
            asset_create_operation,
            asset_update_operation,
            asset_update_bitasset_operation, // [idx: 12]
            asset_update_feed_producers_operation,
            asset_issue_operation,
            asset_reserve_operation,         // [idx: 15]
            asset_fund_fee_pool_operation,
            asset_settle_operation,
            asset_global_settle_operation,   // [idx: 18]
            asset_publish_feed_operation,
            witness_create_operation,
            witness_update_operation,        // [idx: 21]
            proposal_create_operation,
            proposal_update_operation,
            proposal_delete_operation,       // [idx: 24]
            withdraw_permission_create_operation,
            withdraw_permission_update_operation,
            withdraw_permission_claim_operation,  // [idx: 27]
            withdraw_permission_delete_operation,
            committee_member_create_operation,
            committee_member_update_operation,    // [idx: 30]
            committee_member_update_global_parameters_operation,
            vesting_balance_create_operation,
            vesting_balance_withdraw_operation,   // [idx: 33]
            worker_create_operation,
            custom_operation,
            assert_operation,                     // [idx: 36]
            balance_claim_operation,
            override_transfer_operation,
            transfer_to_blind_operation,          // [idx: 39]
            blind_transfer_operation,
            transfer_from_blind_operation,
            asset_settle_cancel_operation,     // VIRTUAL, // [idx: 42]
            asset_claim_fees_operation,
            fba_distribute_operation,          // VIRTUAL
            bonus_operation,                   // [idx: 45]
            daily_issue_operation,             // coin maturing
            referral_issue_operation,
            edc_asset_fund_fee_pool_operation, // [idx: 48]
            account_restrict_operation,        // restrict account money_in, money_out, bonuses
            account_allow_referrals_operation, // account can create other accounts
            set_online_time_operation,         // [idx: 51]
            set_verification_is_required_operation,
            allow_create_asset_operation,
            add_address_operation,             // [idx: 54]
            /**
             * we can't edit old 'asset_update_operation' because of
             * backward compatibility
             */
            asset_update2_operation,  // [idx: 55]
            // funds
            fund_create_operation,
            fund_update_operation,
            fund_refill_operation,    // [idx: 58]
            fund_deposit_operation,
            fund_withdrawal_operation,
            fund_payment_operation,   // [idx: 61]
            fund_set_enable_operation,
            fund_deposit_set_enable_operation,
            fund_remove_operation,     // [idx: 64]
            // -- funds
            allow_create_addresses_operation,
            set_burning_mode_operation,          // [idx: 66]
            assets_update_fee_payer_operation,
            asset_update_exchange_rate_operation // [idx: 68]
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

} } // graphene::chain

FC_REFLECT_TYPENAME( graphene::chain::operation )
FC_REFLECT( graphene::chain::op_wrapper, (op) )
