#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>
#include <boost/range.hpp>

#include <iostream>
#include <graphene/chain/settings_object.hpp>

namespace graphene { namespace chain {

double fund_object::get_rate_percent(const fund_options::fund_rate& fr_item, const database& db) const
{
   int days_passed = (db.head_block_time().sec_since_epoch() - prev_maintenance_time_on_creation.sec_since_epoch()) / 86400;

   // the further away, the 'result' is lower
   double result = db.get_percent(fr_item.day_percent) - (db.get_percent(rates_reduction_per_month) / (double)30 * (double)(days_passed - 1));

   if (result < 0) {
      result = 0;
   }

   return result;
}

optional<fund_options::fund_rate>
fund_object::get_max_fund_rate(const share_type& amnt) const
{
   optional<fund_options::fund_rate> result;
   if (fund_rates.size() == 0) return { result };

   // we need nearest (to the fund_balance) and maximum values
//      // suspicious variant, right?
//      const auto& iter = std::max_element(fund_rates.begin(), fund_rates.end(),
//         [&fund_balance](const fund_options::fund_rate& item1, const fund_options::fund_rate& item2) {
//            return ( (fund_balance < item1.amount) || ((fund_balance >= item2.amount) && (item2.amount > item1.amount)) );
//         });
//      if ( (iter != fund_rates.end()) && (fund_balance >= iter->amount) ) {
//         result = *iter;
//      }

   int max_idx = -1;
   for (uint32_t i = 0; i < fund_rates.size(); ++i)
   {
      if (amnt >= fund_rates[i].amount)
      {
         if (max_idx == -1) {
            max_idx = i;
         }
         else
         {
            if (fund_rates[i].amount > fund_rates[max_idx].amount) {
               max_idx = i;
            }
         }
      }
   }

   if (max_idx > -1) {
      result = fund_rates[max_idx];
   }

   return result;
}

optional<fund_options::payment_rate>
fund_object::get_payment_rate(uint32_t period) const
{
   optional<fund_options::payment_rate> result;

   auto iter = std::find_if(payment_rates.begin(), payment_rates.end(), [&period](const fund_options::payment_rate& item) {
      return (item.period == period);
   });
   if (iter != payment_rates.end()) {
      result = *iter;
   }

   return result;
}

void fund_object::process(database& db) const
{
   const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
   const global_property_object& gpo = db.get_global_properties();
   //const settings_object& settings = *db.find(settings_id_type(0));

   auto asset_itr = db.get_index_type<asset_index>().indices().get<by_id>().find(asset_id);
   assert(asset_itr != db.get_index_type<asset_index>().indices().get<by_id>().end());
   const chain::asset_object& asst = *asset_itr;

   transaction_evaluation_state eval(&db);

   // all payments to users
   share_type daily_payments_without_owner;

   /**
    * create tmp object, because below will be reducing of fund's balance if deposit is
    * overdued (in deposits loop)
    */
   share_type old_balance = balance;

   fund_history_object::history_item h_item;
   h_item.create_datetime = db.head_block_time();

   const auto& users_idx = db.get_index_type<account_index>().indices().get<by_id>();
   std::vector<fund_deposit_id_type> deps_to_remove;

   // find own fund deposits
   auto range = db.get_index_type<fund_deposit_index>().indices().get<by_fund_id>().equal_range(id);
   std::for_each(range.first, range.second, [&](const fund_deposit_object& dep)
   {
      auto user_ptr = users_idx.find(dep.account_id);

      if (dep.enabled && (user_ptr != users_idx.end())) // dep.enabled: important condition for full-history nodes
      {
         const account_object& acc = *user_ptr;
         const optional<fund_options::payment_rate>& p_rate = get_payment_rate(dep.period);

         bool is_valid = (db.head_block_time() >= HARDFORK_626_TIME) ? (dep.daily_payment.value > 0) : p_rate.valid();

         if ( (db.head_block_time() > HARDFORK_627_TIME)
              && (asst.get_id() == EDC_ASSET)
              && !dep.can_use_percent
            ) { is_valid = false; }

         if (is_valid)
         {
            asset asst_quantity;

            if (db.head_block_time() >= HARDFORK_626_TIME) {
               asst_quantity = db.check_supply_overflow(asst.amount(dep.daily_payment));
            }
            else
            {
               share_type quantity = db.get_deposit_daily_payment(dep.percent, p_rate->period, dep.amount.amount);
               if (quantity.value > 0) {
                  asst_quantity = db.check_supply_overflow(asst.amount(quantity));
               }
            }

            if (asst_quantity.amount.value > 0)
            {
               chain::fund_payment_operation op;
               op.issuer = asst.issuer;
               op.fund_id = id;
               op.deposit_id = dep.get_id();
               op.asset_to_issue = asst_quantity;
               op.issue_to_account = dep.account_id;

               try
               {
                  op.validate();
                  db.apply_operation(eval, op);
               } catch (fc::assert_exception& e) { }

               daily_payments_without_owner += asst_quantity.amount;
            }
         }

         // return deposit amount to user and remove deposit if overdue
         if ((dpo.next_maintenance_time - gpo.parameters.maintenance_interval) >= dep.datetime_end)
         {
            bool dep_was_overdue = true;

            if (db.head_block_time() >= HARDFORK_624_TIME)
            {
               if (acc.deposits_autorenewal_enabled)
               {
                  dep_was_overdue = false;

                  if (db.head_block_time() > HARDFORK_625_TIME)
                  {
                     chain::deposit_renewal_operation op;
                     op.account_id = dep.account_id;
                     op.deposit_id = dep.get_id();
                     op.percent = dep.percent;
                     // fund may already have new percents, updating...
                     if (p_rate.valid() && !dep.manual_percent_enabled) {
                        op.percent = p_rate->percent;
                     }

                     if (db.head_block_time() >= HARDFORK_626_TIME)
                     {
                        op.datetime_end = db.get_dynamic_global_properties().next_maintenance_time
                                          - db.get_global_properties().parameters.maintenance_interval + (86400 * dep.period);
                     }
                     else {
                        op.datetime_end = dep.datetime_end + (86400 * dep.period);
                     }

                     try
                     {
                        op.validate();
                        db.apply_operation(eval, op);
                     } catch (fc::assert_exception& e) { }
                  }
                  // last_budget_time - not stable
                  else
                  {
                     db.modify(dep, [&](fund_deposit_object& dep)
                     {
                        if (p_rate.valid()) {
                           dep.percent = p_rate->percent;
                        }
                        dep.datetime_end = db.get_dynamic_global_properties().last_budget_time + (86400 * dep.period);
                     });
                  };
               }
            }

            if (dep_was_overdue)
            {
               // remove deposit
               deps_to_remove.emplace_back(dep.get_id());

               if ( (db.head_block_time() <= HARDFORK_628_TIME)
                    || ((db.head_block_time() > HARDFORK_628_TIME) && (dep.amount.amount > 0)) )
               {
                  // return deposit to user
                  chain::fund_withdrawal_operation op;
                  op.issuer = asst.issuer;
                  op.fund_id = id;
                  op.asset_to_issue = asst.amount(dep.amount.amount);
                  op.issue_to_account = dep.account_id;
                  op.datetime = db.head_block_time();

                  try
                  {
                     op.validate();
                     db.apply_operation(eval, op);
                  } catch (fc::assert_exception& e) {}

                  // reduce fund balance
                  db.modify(*this, [&](chain::fund_object& f) {
                     f.balance -= dep.amount.amount;
                  });

                  // disable deposit
                  db.modify(dep, [&](chain::fund_deposit_object& f) {
                     f.enabled = false;
                  });
               }
            }
         }
      }
   });

   // make payment to fund owner, case 1
   if (payment_scheme == fund_payment_scheme::residual)
   {
      const optional<fund_options::fund_rate>& p_rate = get_max_fund_rate(old_balance);
      if (p_rate.valid())
      {
         share_type fund_day_profit = std::roundl((long double)old_balance.value * get_rate_percent(*p_rate, db));
         if (fund_day_profit > 0)
         {
            h_item.daily_payments_total = fund_day_profit;
            h_item.daily_payments_owner = fund_day_profit - daily_payments_without_owner;
            h_item.daily_payments_without_owner = daily_payments_without_owner;

            share_type owner_profit = fund_day_profit - daily_payments_without_owner;
            const asset& asst_owner_quantity = db.check_supply_overflow(asst.amount(owner_profit));

            // std::cout << "old_balance: " << old_balance.value
            //           << ", fund_deposits_sum: " << fund_deposits_sum.value
            //           << ", owner profit: " << owner_profit.value << std::endl;

            if (asst_owner_quantity.amount.value > 0)
            {
               chain::fund_payment_operation op;
               op.issuer = asst.issuer;
               op.fund_id = id;
               op.asset_to_issue = asst_owner_quantity;
               op.issue_to_account = owner;

               try
               {
                  op.validate();
                  db.apply_operation(eval, op);
               } catch (fc::assert_exception& e) { }
            }
         }
      }
   }
   // case 2
   else if (payment_scheme == fund_payment_scheme::fixed)
   {
      const optional<fund_options::fund_rate>& p_rate = get_max_fund_rate(owner_balance);
      if (p_rate)
      {
         share_type quantity = std::roundl(db.get_percent(p_rate->day_percent) * (long double)daily_payments_without_owner.value);
         const asset& asst_owner_quantity = db.check_supply_overflow(asst.amount(quantity));

         if (asst_owner_quantity.amount.value > 0)
         {
            h_item.daily_payments_total = quantity + daily_payments_without_owner;
            h_item.daily_payments_owner = quantity;
            h_item.daily_payments_without_owner = daily_payments_without_owner;

            chain::fund_payment_operation op;
            op.issuer = asst.issuer;
            op.fund_id = id;
            op.asset_to_issue = asst_owner_quantity;
            op.issue_to_account = owner;

            try
            {
               op.validate();
               db.apply_operation(eval, op);
            } catch (fc::assert_exception& e) { }
         }
      }
      else {
         wlog("Not enough owner balance to make payment to owner. Owner: ${o}, balance: ${b}", ("o", owner)("b", owner_balance));
      }
   }

//   // erase overdued deposits if no full-node
//   if (db.get_history_size() > 0)
//   {
//      for (const fund_deposit_id_type& dep_id: deps_to_remove)
//      {
//         auto itr = db.get_index_type<fund_deposit_index>().indices().get<by_id>().find(dep_id);
//         if (itr != db.get_index_type<fund_deposit_index>().indices().get<by_id>().end()) {
//            db.remove(*itr);
//         }
//      }
//   }

   // erase old history items
   const auto& hist_obj = history_id(db);
   db.modify(hist_obj, [&](fund_history_object& o)
   {
      o.items.emplace_back(std::move(h_item));

      if (db.get_history_size() > 0)
      {
         const time_point& tp = db.head_block_time() - fc::days(db.get_history_size());

         for (auto it = o.items.begin(); it != o.items.end();)
         {
            if (it->create_datetime < tp) {
               it = o.items.erase(it);
            }
            else {
               break;
               //++it;
            }
         }
      }
   });
}

void fund_object::finish(database& db) const
{
   share_type owner_deps = owner_balance;
   if (owner_deps > 0)
   {
      auto asset_itr = db.get_index_type<asset_index>().indices().get<by_id>().find(asset_id);
      assert(asset_itr != db.get_index_type<asset_index>().indices().get<by_id>().end());
      const chain::asset_object& asst = *asset_itr;
      transaction_evaluation_state eval(&db);

      // return amount to owner
      chain::fund_withdrawal_operation op;
      op.issuer           = asst.issuer;
      op.fund_id          = id;
      op.asset_to_issue   = asst.amount(owner_deps);
      op.issue_to_account = owner;
      op.datetime         = db.head_block_time();

      try
      {
         op.validate();
         db.apply_operation(eval, op);
      } catch (fc::assert_exception& e) { }
   }

   // reduce fund balance & disable
   db.modify(*this, [&](chain::fund_object& f)
   {
      if (owner_deps > 0) {
         f.balance -= owner_deps;
      }
      f.owner_balance = 0;
      f.enabled = false;
   });
}

} } // graphene::chain
