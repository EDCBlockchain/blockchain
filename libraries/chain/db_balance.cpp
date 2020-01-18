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

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/settings_object.hpp>

#include <iostream>
#include <boost/range/adaptor/reversed.hpp>

namespace graphene { namespace chain {

asset database::get_balance(account_id_type owner, asset_id_type asset_id) const
{
   auto& index = get_index_type<account_balance_index>().indices().get<by_account_asset>();
   auto itr = index.find(boost::make_tuple(owner, asset_id));
   if( itr == index.end() )
      return asset(0, asset_id);
   return itr->get_balance();
}

asset database::get_balance_for_bonus( account_id_type owner, asset_id_type asset_id )const 
{
   asset_object asset_obj = asset_id( *this );
   if (asset_obj.params.coin_maturing)
   {
      auto& mat_index = get_index_type<account_mature_balance_index>().indices().get<by_account_asset>();
      auto itr = mat_index.find( boost::make_tuple( owner, asset_id ) );
      if ( (itr == mat_index.end()) || ( ( asset_obj.params.mandatory_transfer > 0 ) && !itr->mandatory_transfer ) ) {
         return asset(0, asset_id);
      }
      return itr->get_balance();
   }
   else
   {
      auto& bal_index = get_index_type<account_balance_index>().indices().get<by_account_asset>();
      auto itr = bal_index.find( boost::make_tuple( owner, asset_id ) );
      if ( itr == bal_index.end() || ( ( asset_obj.params.mandatory_transfer > 0 ) && !itr->mandatory_transfer ) ) {
         return asset(0, asset_id);
      }
      auto balance = itr->get_balance();
      auto online_info = get( accounts_online_id_type() ).online_info;
      if (!asset_obj.params.mining || !online_info.size()) return balance;
      auto account_online = online_info.find(owner);
      if (account_online == online_info.end()) {
         return asset( 0, asset_id );
      }
      else
      {
         balance.amount.value *= account_online->second / 1440.0;
         return balance;
      }
   }
}

asset database::get_mature_balance(account_id_type owner, asset_id_type asset_id) const
{
//    auto& owner_account = owner(*this);
   // auto& asset_obj = asset_id(*this);
   auto& index = get_index_type<account_mature_balance_index>().indices().get<by_account_asset>();
   auto itr = index.find(boost::make_tuple(owner, asset_id));
   if( itr == index.end() || !itr->mandatory_transfer ) //  && !owner_account.is_market))
      return asset(0, asset_id);
   return itr->get_balance();
}

asset database::get_balance(const account_object& owner, const asset_object& asset_obj) const
{
   return get_balance(owner.get_id(), asset_obj.get_id());
}

string database::to_pretty_string( const asset& a )const
{
   return a.asset_id(*this).amount_to_pretty_string(a.amount);
}

void database::adjust_balance(account_id_type account, asset delta )
{ try {
   if( delta.amount == 0 )
      return; 
   auto& mat_index = get_index_type<account_mature_balance_index>().indices().get<by_account_asset>();
   auto mat_itr = mat_index.find(boost::make_tuple(account, delta.asset_id));
   double interval_part = 1;
   double interval_length = get_global_properties().parameters.maintenance_interval;
   if (get_dynamic_global_properties().next_maintenance_time >= head_block_time())
      interval_part = ((get_dynamic_global_properties().next_maintenance_time.sec_since_epoch() - head_block_time().sec_since_epoch())) / interval_length;
   if (interval_part > 1) interval_part = 1;

   auto& index = get_index_type<account_balance_index>().indices().get<by_account_asset>();
   auto itr = index.find(boost::make_tuple(account, delta.asset_id));
   if(itr == index.end())
   {
      FC_ASSERT( delta.amount > 0, "Insufficient Balance: ${a}'s balance of ${b} is less than required ${r}", 
                 ("a",account(*this).name)
                 ("b",to_pretty_string(asset(0,delta.asset_id)))
                 ("r",to_pretty_string(-delta)));
      create<account_balance_object>([account,&delta](account_balance_object& b) {
         b.owner = account;
         b.asset_type = delta.asset_id;
         b.balance = delta.amount.value;
      });

      // disable maturity for EDC
      if ( ((delta.asset_id != EDC_ASSET)
            || ((delta.asset_id == EDC_ASSET) && (head_block_time() < HARDFORK_622_TIME))) )
      {
         create<account_mature_balance_object>(
         [this, account, delta, &interval_part](account_mature_balance_object& b) {
            b.owner = account;
            b.asset_type = delta.asset_id;
            b.balance = delta.amount.value * interval_part;
            b.history.push_back(mature_balances_history(delta.amount, b.balance));
         });
      }
   }
   else
   {
      if( delta.amount < 0 )
      {
         FC_ASSERT(itr->get_balance() >= -delta,
                   "Insufficient Balance: ${a}'s balance of ${b} is less than required ${r}",
                   ("a", account(*this).name)
                   ("b", to_pretty_string(itr->get_balance()))
                   ("r", to_pretty_string(-delta)));

      }
      auto asset_params = delta.asset_id(*this).params;
      modify(*itr, [delta, asset_params](account_balance_object& b)
      {
         if (delta.amount.value <= -asset_params.mandatory_transfer) {
            b.mandatory_transfer = true;
         } 
         b.adjust_balance(delta);
      });
      int64_t value = delta.amount.value;
      if (value > 0)
         value *= interval_part;

      if (mat_itr != mat_index.end())
      {
         modify(*mat_itr, [&](account_mature_balance_object& b) {
            b.adjust_balance(asset(value, delta.asset_id), delta, asset_params.mandatory_transfer);
         });
      }
   }
} FC_CAPTURE_AND_RETHROW( (account)(delta) ) }

void database::adjust_bonus_balance(account_id_type account, asset delta) 
{
   auto& index = get_index_type<bonus_balances_index>().indices().get<by_account>();
   auto bonus_balances_itr = index.find(account);

   if (bonus_balances_itr == index.end())
   {
      // previously called function 'check_supply_overflow()' can return 0, so...
      if (delta.amount > 0)
      {
         // disable for EDC
         if ((head_block_time() > HARDFORK_622_TIME) && (delta.asset_id == EDC_ASSET)) { return; }

         create<bonus_balances_object>([&](bonus_balances_object& bbo) {
            bbo.owner = account;
            bbo.adjust_balance(delta, head_block_time(), this);
         });
      }
   }
   else
   {
      modify<bonus_balances_object>(*bonus_balances_itr, [&](bonus_balances_object& bbo) {
         bbo.adjust_balance(delta, head_block_time(), this);
      });
   }
}

void database::adjust_bonus_balance(account_id_type account, referral_balance_info ref_info)
{
   auto& index = get_index_type<bonus_balances_index>().indices().get<by_account>();
   auto bonus_balances_itr = index.find(account);
   if (bonus_balances_itr == index.end())
   {
      if (head_block_time() > HARDFORK_622_TIME) { return; }

      create<bonus_balances_object>([&](bonus_balances_object& bbo) {
         bbo.owner = account;
         bbo.add_referral(ref_info, head_block_time(), this);
      });
   }
   else
   {
      modify<bonus_balances_object>(*bonus_balances_itr, [&](bonus_balances_object& bbo) {
         bbo.add_referral(ref_info, head_block_time(), this);
      });
   }
}

void database::process_bonus_balances(account_id_type account_id)
{
   auto& index = get_index_type<bonus_balances_index>().indices().get<by_account>();
   auto bonus_balances_itr = index.find(account_id);
   if (bonus_balances_itr == index.end()) { return; }
   auto check_time = head_block_time() > HARDFORK_620_FIX_TIME 
                        ? head_block_time() - fc::days(30)
                        : head_block_time() - fc::days(3);
   const std::vector<bonus_balances_object::bonus_balances_info>& matured_balances = bonus_balances_itr->balances_before_date(check_time);

   const auto edc_asset = get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
   auto edc_balance = get_balance(account_id, edc_asset->get_id()).amount;

   transaction_evaluation_state eval(this);

   for (const bonus_balances_object::bonus_balances_info& balance_info: matured_balances)
   {
      if (!balance_info.balances.size() && !balance_info.referral.quantity) { continue; }

      if (head_block_time() < HARDFORK_621_TIME)
      {
         chain::referral_issue_operation r_op;
         r_op.issuer = edc_asset->issuer;
         r_op.asset_to_issue = check_supply_overflow( edc_asset->amount( balance_info.referral.quantity ) );
         r_op.issue_to_account = account_id;
         r_op.account_balance = edc_balance;
         r_op.history = balance_info.referral.history;
         r_op.rank = balance_info.referral.rank;
         try {
            r_op.validate();
            apply_operation(eval, r_op);
         } catch (fc::assert_exception& e) { }
      }

      for (auto blns : balance_info.balances) {
         auto account_balance = get_balance(account_id, blns.first).amount;
         chain::daily_issue_operation op;
         op.issuer = blns.first(*this).issuer;
         op.asset_to_issue = check_supply_overflow( asset( blns.second, blns.first ) );
         op.issue_to_account = account_id;
         op.account_balance = account_balance;
         try {
            op.validate();
            apply_operation(eval, op);
         } catch (fc::assert_exception& e) {  }
      }
   }

   modify<bonus_balances_object>(*bonus_balances_itr, [&](bonus_balances_object& bbo) {
      bbo.remove(check_time);
   });
}

address database::get_address() {
    return address(_current_block_num, _current_trx_in_block);
}

void database::consider_mining_in_mature_balances()
{
   if (!find(accounts_online_id_type())) { return; }

   auto& online_info = get(accounts_online_id_type()).online_info;
   if ( !online_info.size() ) { return; }

   const auto& asset_idx = get_index_type<asset_index>();
   const auto& account_idx = get_index_type<chain::account_index>();
   asset_idx.inspect_all_objects( [&](const object& obj)
   {
      const asset_object& asset = static_cast<const asset_object&>( obj );
      if ( asset.id == asset_id_type(0) ) { return; }
      if (!asset.params.daily_bonus || !asset.params.mining ) { return; }

      account_idx.inspect_all_objects( [&]( const object& obj )
      {
         const account_object& account = static_cast<const account_object&>( obj );
         uint16_t mined_minutes = 0;

         try
         {
            auto iter = online_info.find(account.get_id());
            if (iter != online_info.end()) {
               mined_minutes = iter->second;
            }
         }
         catch (...) {
            wlog("exception, ${file}:${func}", ("file", __FILE__)("func", __PRETTY_FUNCTION__));
         }

         auto& mat_index = get_index_type<account_mature_balance_index>().indices().get<by_account_asset>();
         auto mat_itr = mat_index.find( boost::make_tuple( account.get_id(), asset.get_id() ) );
         if ( mat_itr == mat_index.end() ) { return; }
         modify( *mat_itr, [mined_minutes]( account_mature_balance_object& b ) {
            b.consider_mining( mined_minutes );
         });
      });
   });
}

void database::consider_mining_old() 
{
   auto& online_info = get( accounts_online_id_type() ).online_info;
   if (!online_info.size()) return;
   const auto& account_idx = get_index_type<chain::account_index>();
   const auto asset = get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
   account_idx.inspect_all_objects( [&](const chain::object& obj) {
      const chain::account_object& account = static_cast<const chain::account_object&>(obj);
      uint16_t mined_minutes = 0;
      try {
         mined_minutes = online_info.at(account.get_id());
      } catch(...) { }
      //auto balance = get_mature_balance(account.get_id(), asset->get_id()).amount;
      auto& mat_index = get_index_type<account_mature_balance_index>().indices().get<by_account_asset>();
      auto mat_itr = mat_index.find(boost::make_tuple(account.get_id(), asset->get_id()));
      if (mat_itr == mat_index.end()) return;
      modify(*mat_itr, [mined_minutes](account_mature_balance_object& b) {
         b.consider_mining(mined_minutes);
      });
   });
}

asset database::check_supply_overflow( asset value ) 
{
    const asset_object& a = value.asset_id(*this);

    auto asset_dyn_data = &a.dynamic_asset_data_id(*this);
    if ((asset_dyn_data->current_supply + value.amount) > a.options.max_supply ) {
        return asset( a.options.max_supply - asset_dyn_data->current_supply, value.asset_id );
    }
    return value;
}

share_type database::get_user_deposits_sum(account_id_type acc_id, asset_id_type asset_id)
{
   share_type amount = 0;

   const auto& idx_funds = get_index_type<fund_index>().indices().get<by_id>();
   for (const fund_object& fund_obj: idx_funds)
   {
      if (fund_obj.asset_id == asset_id)
      {
         const auto& stats_obj = fund_obj.get_id()(*this).statistics_id(*this);
         auto iter = stats_obj.users_deposits.find(acc_id);
         if (iter != stats_obj.users_deposits.end()) {
            amount += iter->second;
         }
      }
   }

   return amount;
}

double database::get_percent(uint32_t percent) const {
   return percent / 100000.0;
}

share_type database::get_deposit_daily_payment(uint32_t percent, uint32_t period, share_type amount) const
{
   double percent_per_day = get_percent(percent) / period;
   return (share_type)std::roundl(percent_per_day * (long double)amount.value);
}

void database::rebuild_user_edc_deposit_availability(account_id_type acc_id)
{
   const settings_object& settings = *find(settings_id_type(0));

   const auto& accounts_by_id = get_index_type<account_index>().indices().get<by_id>();
   auto itr = accounts_by_id.find(acc_id);
   if (itr == accounts_by_id.end()) { return; }
   const account_object& acc = *itr;

   if (acc.edc_in_deposits > settings.edc_deposit_max_sum)
   {
      const auto& range = get_index_type<fund_deposit_index>().indices().get<by_account_id>().equal_range(acc_id);
      share_type amount_count;

      // sorting should be in order of addition ...
      for (const fund_deposit_object& item: boost::make_iterator_range(range.first, range.second))
      {
         if (!item.enabled) { continue; }
         if (item.manual_percent_enabled) { continue; }
         if (item.amount.asset_id != EDC_ASSET) { continue; }

         amount_count += item.amount.amount;
         bool can_use_percent = (amount_count < settings.edc_deposit_max_sum) ? true : false;

         modify(item, [&](fund_deposit_object& obj) {
            obj.can_use_percent = can_use_percent;
         });
      }
   }
}

void database::issue_referral()
{
   const auto& idx = get_index_type<chain::account_index>();
   const auto edc_asset = get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
   const auto& bal_idx = get_index_type<account_balance_index>();
   auto& mat_bal_idx = get_index_type<account_mature_balance_index>();   
   auto& issuer_list = edc_asset->issuer( *this ).blacklisted_accounts;
   auto& alpha_list = ALPHA_ACCOUNT_ID( *this ).blacklisted_accounts;
   int minutes_in_1_day = 1440;
   auto online_info = get( accounts_online_id_type() ).online_info;
   double default_online_part = online_info.size() ? 0 : 1;
   referral_tree rtree( idx, bal_idx, edc_asset->id, account_id_type(), &mat_bal_idx );
   rtree.form();
   auto ops = rtree.scan();

   transaction_evaluation_state eval(this);

   for( const chain::referral_info& op_info : ops )
   {
      if (  alpha_list.count( op_info.to_account_id ) ) { continue; }
      if ( issuer_list.count( op_info.to_account_id ) ) { continue; }

      if (head_block_time() > HARDFORK_621_TIME) { continue; }

      if ( head_block_time() > HARDFORK_620_TIME ) {
         adjust_bonus_balance( op_info.to_account_id, referral_balance_info( op_info.quantity,  op_info.rank, op_info.history ) );
      }
      else
      {
         auto real_balance =   get_balance(op_info.to_account_id, edc_asset->get_id()).amount;
         auto balance = get_balance_for_bonus(op_info.to_account_id, edc_asset->get_id()).amount;
         double online_part = default_online_part;

         if ( (head_block_time() > HARDFORK_618_TIME) && (head_block_time() < HARDFORK_619_TIME) && (default_online_part == 0) )
         {
            try {
               online_part = online_info.at(op_info.to_account_id) / (double)minutes_in_1_day;
            } catch( ... ) { }
         }
         if ( (head_block_time() < HARDFORK_620_TIME) && ( balance.value * 0.0065 * online_part < 1 ) ) { continue; }

         chain::referral_issue_operation r_op;
         r_op.issuer = edc_asset->issuer;
         r_op.asset_to_issue = check_supply_overflow( 
                                        edc_asset->amount( head_block_time() > HARDFORK_618_TIME && head_block_time() < HARDFORK_619_TIME ? 
                                            op_info.quantity * online_part 
                                            : 
                                            op_info.quantity
                                        )
                                );
         r_op.issue_to_account = op_info.to_account_id;
         r_op.account_balance = real_balance;
         r_op.history = op_info.history;
         r_op.rank = op_info.rank;
         try {
            r_op.validate();
            apply_operation( eval, r_op );
         } catch ( fc::assert_exception& e ) { }
      }
   }
}

fc::optional< vesting_balance_id_type > database::deposit_lazy_vesting(
   const fc::optional< vesting_balance_id_type >& ovbid,
   share_type amount, uint32_t req_vesting_seconds,
   account_id_type req_owner,
   bool require_vesting )
{
   if( amount == 0 )
      return fc::optional< vesting_balance_id_type >();

   fc::time_point_sec now = head_block_time();

   while( true )
   {
      if( !ovbid.valid() )
         break;
      const vesting_balance_object& vbo = (*ovbid)(*this);
      if( vbo.owner != req_owner )
         break;
      if( vbo.policy.which() != vesting_policy::tag< cdd_vesting_policy >::value )
         break;
      if( vbo.policy.get< cdd_vesting_policy >().vesting_seconds != req_vesting_seconds )
         break;
      modify( vbo, [&]( vesting_balance_object& _vbo )
      {
         if( require_vesting )
            _vbo.deposit(now, amount);
         else
            _vbo.deposit_vested(now, amount);
      } );
      return fc::optional< vesting_balance_id_type >();
   }

   const vesting_balance_object& vbo = create< vesting_balance_object >( [&]( vesting_balance_object& _vbo )
   {
      _vbo.owner = req_owner;
      _vbo.balance = amount;

      cdd_vesting_policy policy;
      policy.vesting_seconds = req_vesting_seconds;
      policy.coin_seconds_earned = require_vesting ? 0 : amount.value * policy.vesting_seconds;
      policy.coin_seconds_earned_last_update = now;

      _vbo.policy = policy;
   } );

   return vbo.id;
}

void database::deposit_cashback(const account_object& acct, share_type amount, bool require_vesting)
{
   // If we don't have a VBO, or if it has the wrong maturity
   // due to a policy change, cut it loose.

   if( amount == 0 )
      return;

   if( acct.get_id() == GRAPHENE_COMMITTEE_ACCOUNT || acct.get_id() == GRAPHENE_WITNESS_ACCOUNT ||
       acct.get_id() == GRAPHENE_RELAXED_COMMITTEE_ACCOUNT || acct.get_id() == GRAPHENE_NULL_ACCOUNT ||
       acct.get_id() == GRAPHENE_TEMP_ACCOUNT )
   {
      // The blockchain's accounts do not get cashback; it simply goes to the reserve pool.
      modify(get(asset_id_type()).dynamic_asset_data_id(*this), [amount](asset_dynamic_data_object& d) {
         d.current_supply -= amount;
      });
      return;
   }

   fc::optional< vesting_balance_id_type > new_vbid = deposit_lazy_vesting(
      acct.cashback_vb,
      amount,
      get_global_properties().parameters.cashback_vesting_period_seconds,
      acct.id,
      require_vesting );

   if( new_vbid.valid() )
   {
      modify( acct, [&]( account_object& _acct )
      {
         _acct.cashback_vb = *new_vbid;
      } );
   }

   return;
}

void database::deposit_witness_pay(const witness_object& wit, share_type amount)
{
   if( amount == 0 )
      return;

   fc::optional< vesting_balance_id_type > new_vbid = deposit_lazy_vesting(
      wit.pay_vb,
      amount,
      get_global_properties().parameters.witness_pay_vesting_seconds,
      wit.witness_account,
      true );

   if( new_vbid.valid() )
   {
      modify( wit, [&]( witness_object& _wit )
      {
         _wit.pay_vb = *new_vbid;
      } );
   }

   return;
}

} }
