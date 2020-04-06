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
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>
#include <iostream>

namespace graphene { namespace chain {

share_type cut_fee(share_type a, uint16_t p)
{
   if( a == 0 || p == 0 )
      return 0;
   if( p == GRAPHENE_100_PERCENT )
      return a;

   fc::uint128_t r = a.value;
   r *= p;
   r /= GRAPHENE_100_PERCENT;
   return static_cast<uint64_t>(r);
}

void account_balance_object::adjust_balance(const asset& delta)
{
   assert(delta.asset_id == asset_type);
   balance += delta.amount;
}

void account_mature_balance_object::adjust_balance(const asset& delta, const asset& real_balance, int64_t mandatory)
{
   assert(delta.asset_id == asset_type);
   int64_t value = delta.amount.value;
   if (value < 0) {
      if (value <= -mandatory) {
          mandatory_transfer = true;
      } 
      while (value < 0) {
         auto& back = history.back();
         int64_t coins_to_remove = 0;
         if ( value + back.real_balance > 0) {
            double part_to_take = - value / (double)back.real_balance.value;
           
            if (part_to_take > 1) part_to_take = 1;
            coins_to_remove += back.balance.value * part_to_take;
            value += back.real_balance.value * part_to_take;
            back.real_balance -= back.real_balance * part_to_take;
            back.balance -= back.balance * part_to_take;
            if (back.real_balance == 0) history.erase(--history.end());
         } else {
            coins_to_remove += back.balance.value;
            value += back.real_balance.value;
            history.erase(--history.end());
         }
         balance -= coins_to_remove;
      }
   } else {
      balance += delta.amount;
      history.push_back(mature_balances_history(real_balance.amount, delta.amount));
   }
}

string bonus_balances_object::get_date(time_point_sec time_point) const {
    return time_point.to_iso_string().substr(0, 10);
}

int bonus_balances_object::get_pos_by_date(time_point_sec time_point) const
{
    int pos = -1;
    string date_string = get_date(time_point);
    for (int i = 0; i < (int)balances_by_date.size(); i++) {
        if (get_date(balances_by_date[i].bonus_time) == date_string) {
            pos = i;
            break;
        }
    }
    return pos;
}

asset bonus_balances_object::get_daily_balance(asset_id_type asset_id, time_point_sec time_point) const
{
    const int pos = (time_point != time_point_sec()) ?
                        get_pos_by_date(time_point)  : 0;
    if (pos < 0) return asset(0, asset_id);
    auto balances = balances_by_date[pos].balances;
    auto balance_itr = balances.find(asset_id);
    if (balance_itr == balances.end()) {
        return asset(0, asset_id);
    } else {
        return asset(balance_itr->second, asset_id);
    }
}

asset bonus_balances_object::get_referral_balance(asset_id_type asset_id, time_point_sec time_point) const
{
    if (referral_balance_asset != asset_id) return asset(0, asset_id);
    const int pos = (time_point != time_point_sec()) ?
                        get_pos_by_date(time_point)  : 0;
    if (pos < 0) return asset(0, asset_id);
    auto balances_obj = balances_by_date[pos];
    
    return asset(balances_obj.referral.quantity, asset_id);
}

asset bonus_balances_object::get_full_balance(asset_id_type asset_id, time_point_sec time_point) const {
    return get_daily_balance(asset_id, time_point) + get_referral_balance(asset_id, time_point);
}

void bonus_balances_object::adjust_balance(const asset& delta, time_point_sec time_point, database* db)
{
    const int pos = (time_point != time_point_sec()) ? get_pos_by_date(time_point)  : 0;

    if (!balances_by_date.size() || pos == -1)
    {
       if ( (db->head_block_time() <= HARDFORK_622_TIME)
             || ((db->head_block_time() > HARDFORK_622_TIME) && (delta.asset_id != EDC_ASSET)) )
       {
          balances_by_date.insert(balances_by_date.begin(), bonus_balances_info(time_point));
          balances_by_date[0].balances.emplace(delta.asset_id, delta.amount);
       }
       return;
    }
    if (pos < 0) return;

    // increase balance in cases when maintenance_time interval is more often then 1 per 24 hours
    balances_by_date[pos].balances[delta.asset_id] += delta.amount;
}

void  bonus_balances_object::add_referral(referral_balance_info rbi, time_point_sec time_point, database* db)
{
    const int pos = (time_point != time_point_sec()) ? get_pos_by_date(time_point) : 0;

    if ( (!balances_by_date.size() || pos == -1)
          && (db->head_block_time() < HARDFORK_622_TIME) ) // (referral system is already disabled)
    {
        balances_by_date.insert(balances_by_date.begin(), bonus_balances_info(time_point));
        balances_by_date[0].referral = rbi;
        return;
    }
    if (pos < 0) return;

    balances_by_date[pos].referral = rbi;
}

bonus_balances_object::bonus_balances_info bonus_balances_object::get_balances_by_date(time_point_sec time_point) const
{
    int pos = -1;
    string date_string = get_date(time_point);
    if (time_point != time_point_sec()) {
        for (int i = 0; i < (int)balances_by_date.size(); i++) {
            if (get_date(balances_by_date[i].bonus_time) == date_string) {
                pos = i;
                break;
            }
        }
    }
    if (pos < 0) return bonus_balances_object::bonus_balances_info();
    return balances_by_date[pos];
}

vector<bonus_balances_object::bonus_balances_info> bonus_balances_object::balances_before_date(time_point_sec time) const
{
    vector<bonus_balances_object::bonus_balances_info> result;
    if (time == time_point_sec()) return result;
    string date_string = get_date(time);
    string zero_time = "T00:00:00";
    for (auto& balance: balances_by_date)
    {
        if( time_point::from_iso_string(get_date(balance.bonus_time) + zero_time)
                <=
            time_point::from_iso_string(date_string + zero_time) ) {
           result.push_back(balance);
        }
    }
    return result;
}

void bonus_balances_object::remove(time_point_sec time) 
{
    if (!balances_by_date.size()) return;

    if (time == time_point_sec()) return;
    string date_string = get_date(time);
    string zero_time = "T00:00:00";
    
    balances_by_date.erase(std::remove_if(balances_by_date.begin(), balances_by_date.end(), [&] (bonus_balances_object::bonus_balances_info balance) {
        return (time_point::from_iso_string(get_date(balance.bonus_time) + zero_time)
                    <=
                time_point::from_iso_string(date_string + zero_time));
    }), balances_by_date.end());
}

void account_mature_balance_object::consider_mining(uint16_t minied_minutes) {
    double minutes_in_day = 1440;
    balance = balance.value * (minied_minutes / minutes_in_day);
}

void account_statistics_object::process_fees(const account_object& a, database& d) const
{
   if( pending_fees > 0 || pending_vested_fees > 0 )
   {
      auto pay_out_fees = [&](const account_object& account, share_type core_fee_total, bool require_vesting)
      {
         // Check the referrer -- if he's no longer a member, pay to the lifetime referrer instead.
         // No need to check the registrar; registrars are required to be lifetime members.
        //  if( account.referrer(d).is_basic_account(d.head_block_time()) )
        //     d.modify(account, [](account_object& a) {
        //        a.referrer = a.lifetime_referrer;
        //     });

         share_type network_cut = cut_fee(core_fee_total, account.network_fee_percentage);
         assert( network_cut <= core_fee_total );

#ifndef NDEBUG
         const auto& props = d.get_global_properties();

         share_type reserveed = cut_fee(network_cut, props.parameters.reserve_percent_of_fee);
         share_type accumulated = network_cut - reserveed;
         assert( accumulated + reserveed == network_cut );
#endif
         share_type lifetime_cut = cut_fee(core_fee_total, account.lifetime_referrer_fee_percentage);
         share_type referral = core_fee_total - network_cut - lifetime_cut;

         d.modify(asset_dynamic_data_id_type()(d), [network_cut](asset_dynamic_data_object& d) {
            d.accumulated_fees += network_cut;
         });

         // Potential optimization: Skip some of this math and object lookups by special casing on the account type.
         // For example, if the account is a lifetime member, we can skip all this and just deposit the referral to
         // it directly.
         share_type referrer_cut = cut_fee(referral, account.referrer_rewards_percentage);
         share_type registrar_cut = referral - referrer_cut;

         d.deposit_cashback(d.get(account.lifetime_referrer), lifetime_cut, require_vesting);
         d.deposit_cashback(d.get(account.referrer), referrer_cut, require_vesting);
         d.deposit_cashback(d.get(account.registrar), registrar_cut, require_vesting);

         assert( referrer_cut + registrar_cut + accumulated + reserveed + lifetime_cut == core_fee_total );
      };

      pay_out_fees(a, pending_fees, true);
      pay_out_fees(a, pending_vested_fees, false);

      d.modify(*this, [&](account_statistics_object& s) {
         s.lifetime_fees_paid += pending_fees + pending_vested_fees;
         s.pending_fees = 0;
         s.pending_vested_fees = 0;
      });
   }
}

void account_statistics_object::pay_fee( share_type core_fee, share_type cashback_vesting_threshold )
{
   if( core_fee > cashback_vesting_threshold )
      pending_fees += core_fee;
   else
      pending_vested_fees += core_fee;
}

set<account_id_type> account_member_index::get_account_members(const account_object& a)const
{
   set<account_id_type> result;
   for( auto auth : a.owner.account_auths )
      result.insert(auth.first);
   for( auto auth : a.active.account_auths )
      result.insert(auth.first);
   return result;
}
set<public_key_type> account_member_index::get_key_members(const account_object& a)const
{
   set<public_key_type> result;
   for( auto auth : a.owner.key_auths )
      result.insert(auth.first);
   for( auto auth : a.active.key_auths )
      result.insert(auth.first);
   result.insert( a.options.memo_key );
   return result;
}
set<address> account_member_index::get_address_members(const account_object& a)const
{
   set<address> result;
   for (auto auth: a.owner.address_auths) {
      result.insert(auth.first);
   }
   for (auto auth: a.active.address_auths) {
      result.insert(auth.first);
   }
   for (auto addr: a.addresses) {
      result.insert(addr);
   }

   result.insert( a.options.memo_key );
   return result;
}

void account_member_index::object_inserted(const object& obj)
{
    assert( dynamic_cast<const account_object*>(&obj) ); // for debug only
    const account_object& a = static_cast<const account_object&>(obj);

    auto account_members = get_account_members(a);
    for (auto item: account_members) {
       account_to_account_memberships[item].insert(obj.id);
    }

    auto key_members = get_key_members(a);
    for (auto item: key_members) {
       account_to_key_memberships[item].insert(obj.id);
    }

    auto address_members = get_address_members(a);
    for (auto item: address_members) {
       account_to_address_memberships[item].insert(obj.id);
    }
}

void account_member_index::object_removed(const object& obj)
{
    assert( dynamic_cast<const account_object*>(&obj) ); // for debug only
    const account_object& a = static_cast<const account_object&>(obj);

    auto key_members = get_key_members(a);
    for (auto item: key_members) {
       account_to_key_memberships[item].erase( obj.id );
    }

    auto address_members = get_address_members(a);
    for (auto item : address_members) {
       account_to_address_memberships[item].erase( obj.id );
    }

    auto account_members = get_account_members(a);
    for (auto item : account_members) {
       account_to_account_memberships[item].erase( obj.id );
    }
}

void account_member_index::about_to_modify(const object& before)
{
   before_key_members.clear();
   before_account_members.clear();
   assert( dynamic_cast<const account_object*>(&before) ); // for debug only
   const account_object& a = static_cast<const account_object&>(before);
   before_key_members     = get_key_members(a);
   before_address_members = get_address_members(a);
   before_account_members = get_account_members(a);
}

void account_member_index::object_modified(const object& after)
{
    assert( dynamic_cast<const account_object*>(&after) ); // for debug only
    const account_object& a = static_cast<const account_object&>(after);

    {
       set<account_id_type> after_account_members = get_account_members(a);
       vector<account_id_type> removed; removed.reserve(before_account_members.size());
       std::set_difference(before_account_members.begin(), before_account_members.end(),
                           after_account_members.begin(), after_account_members.end(),
                           std::inserter(removed, removed.end()));

       for( auto itr = removed.begin(); itr != removed.end(); ++itr )
          account_to_account_memberships[*itr].erase(after.id);

       vector<object_id_type> added; added.reserve(after_account_members.size());
       std::set_difference(after_account_members.begin(), after_account_members.end(),
                           before_account_members.begin(), before_account_members.end(),
                           std::inserter(added, added.end()));

       for( auto itr = added.begin(); itr != added.end(); ++itr )
          account_to_account_memberships[*itr].insert(after.id);
    }

    {
       set<public_key_type> after_key_members = get_key_members(a);

       vector<public_key_type> removed; removed.reserve(before_key_members.size());
       std::set_difference(before_key_members.begin(), before_key_members.end(),
                           after_key_members.begin(), after_key_members.end(),
                           std::inserter(removed, removed.end()));

       for( auto itr = removed.begin(); itr != removed.end(); ++itr )
          account_to_key_memberships[*itr].erase(after.id);

       vector<public_key_type> added; added.reserve(after_key_members.size());
       std::set_difference(after_key_members.begin(), after_key_members.end(),
                           before_key_members.begin(), before_key_members.end(),
                           std::inserter(added, added.end()));

       for( auto itr = added.begin(); itr != added.end(); ++itr )
          account_to_key_memberships[*itr].insert(after.id);
    }

    {
       set<address> after_address_members = get_address_members(a);

       vector<address> removed; removed.reserve(before_address_members.size());
       std::set_difference(before_address_members.begin(), before_address_members.end(),
                           after_address_members.begin(), after_address_members.end(),
                           std::inserter(removed, removed.end()));

       for (auto itr = removed.begin(); itr != removed.end(); ++itr) {
          account_to_address_memberships[*itr].erase(after.id);
       }

       vector<address> added; added.reserve(after_address_members.size());
       std::set_difference(after_address_members.begin(), after_address_members.end(),
                           before_address_members.begin(), before_address_members.end(),
                           std::inserter(added, added.end()));

       for (auto itr = added.begin(); itr != added.end(); ++itr) {
          account_to_address_memberships[*itr].insert(after.id);
       }
    }

}

void account_referrer_index::object_inserted( const object& obj ) { }
void account_referrer_index::object_removed( const object& obj ) { }
void account_referrer_index::about_to_modify( const object& before ) { }
void account_referrer_index::object_modified( const object& after  ) { }

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::account_object,
                                (graphene::db::object),
                                (membership_expiration_date)
                                (registrar)(referrer)(lifetime_referrer)
                                (network_fee_percentage)(lifetime_referrer_fee_percentage)(referrer_rewards_percentage)
                                (name)(owner)(active)(addresses)(options)(statistics)(whitelisting_accounts)(blacklisting_accounts)(deposit_sums)
                                (whitelisted_accounts)(blacklisted_accounts)
                                (cashback_vb)
                                (owner_special_authority)(active_special_authority)
                                (top_n_control_flags)
                                (allowed_assets)
                                (register_datetime)(last_generated_address)
                                (current_restriction)(is_market)(verification_is_required)
                                (can_create_and_update_asset)(can_create_addresses)
                                (burning_mode_enabled)
                                (deposits_autorenewal_enabled)
                                (edc_in_deposits)
                                (edc_limit_daily_volume_enabled)
                                (edc_transfers_daily_amount_counter)
                              )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::account_balance_object,
                                (graphene::db::object),
                                (owner)(asset_type)(balance)(mandatory_transfer)
                              )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::account_statistics_object,
                                (graphene::chain::object),
                                (owner)(most_recent_op)(total_ops)(total_core_in_orders)
                                (lifetime_fees_paid)(pending_fees)(pending_vested_fees)
                              )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::restricted_account_object,
                                (graphene::db::object),
                                (account)(restriction_type)
                              )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::accounts_online_object,
                                (graphene::db::object),
                                (online_info)
                              )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::market_address_object,
                                (graphene::db::object),
                                (market_account_id)
                                (addr)
                                (notes)
                                (create_datetime)
                              )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::blind_transfer2_object,
                                (graphene::db::object),
                                (from)(to)(amount)(datetime)(memo)(fee)
                              )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::bonus_balances_object,
                                (graphene::db::object),
                                (owner)(balances_by_date)
                              )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::account_mature_balance_object,
                                (graphene::db::object),
                                (owner)(asset_type)(balance)(history)(mandatory_transfer)
                              )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::account_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::restricted_account_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::accounts_online_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::market_address_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::blind_transfer2_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::bonus_balances_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::account_balance_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::account_mature_balance_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::account_statistics_object )