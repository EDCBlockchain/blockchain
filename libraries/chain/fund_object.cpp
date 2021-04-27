#include <iostream>

#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/hardfork.hpp>
//#include <graphene/chain/settings_object.hpp>

#include <fc/uint128.hpp>
#include <boost/range.hpp>

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

   // we need nearest (to the amnt) and maximum values
//      // suspicious variant, right?
//      const auto& iter = std::max_element(fund_rates.begin(), fund_rates.end(),
//         [&amnt](const fund_options::fund_rate& item1, const fund_options::fund_rate& item2) {
//            return ( (item1.amount > amnt) || ((amnt >= item2.amount) && (item2.amount > item1.amount)) );
//         });
//      if ( (iter != fund_rates.end()) && (amnt >= iter->amount) ) {
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

   // all user deposits
   share_type all_deposits = 0;
   if (db.head_block_time() < HARDFORK_640_TIME) {
      all_deposits = balance_before_hf640 - owner_balance;
   }
   else {
      all_deposits = balance - owner_balance;
   }

   /**
    * create tmp object, because below will be reducing of fund's balance if deposit is
    * overdued (in deposits loop)
    */
   share_type old_balance = 0;
   if (db.head_block_time() < HARDFORK_640_TIME) {
      old_balance = balance_before_hf640;
   }
   else {
      old_balance = balance;
   }

   fund_history_object::history_item h_item;
   h_item.create_datetime = db.head_block_time();

   const auto& users_idx = db.get_index_type<account_index>().indices().get<by_id>();
   fc::time_point_sec maint_time = dpo.next_maintenance_time - gpo.parameters.maintenance_interval;

   // find own fund deposits
   auto range = db.get_index_type<fund_deposit_index>().indices().get<by_fund_id>().equal_range(id);
   std::for_each(range.first, range.second, [&](const fund_deposit_object& dep)
   {
      auto user_ptr = users_idx.find(dep.account_id);
      const account_object& acc = *user_ptr;
      bool dep_datetime_end_reached = maint_time >= dep.datetime_end;

      // clear disabled and overdued deposits
      if ((db.head_block_time() >= HARDFORK_638_TIME) && !dep.enabled && dep_datetime_end_reached)
      {
         if (db.head_block_time() >= HARDFORK_640_TIME)
         {
            if (!dep.finished)
            {
               db.modify(dep, [&](chain::fund_deposit_object& obj) {
                  obj.finished = true;
               });
               // reduce fund balance
               db.modify(*this, [&](chain::fund_object& obj) {
                  obj.balance -= dep.amount.amount;
               });
            }
         }
         else
         {
            if (!dep.finished)
            {
               db.modify(dep, [&](chain::fund_deposit_object& obj) {
                  obj.finished = true;
               });
               db.modify(*this, [&](chain::fund_object& obj) {
                  obj.balance -= dep.amount.amount;
               });
            }
            db.modify(*this, [&](chain::fund_object& obj) {
               obj.balance_before_hf640 -= dep.amount.amount;
            });
         }
      }

      if (dep.enabled && !dep.finished && (user_ptr != users_idx.end())) // dep.enabled: important condition for full-history nodes
      {
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

         // return deposit amount to user and remove deposit if overdued
         if (dep_datetime_end_reached)
         {
            bool dep_was_overdued = true;

            if (acc.deposits_autorenewal_enabled && (db.head_block_time() >= HARDFORK_624_TIME))
            {
               dep_was_overdued = false;

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
               }
            }

            if (dep_was_overdued)
            {
               // disable deposit
               db.modify(dep, [&](chain::fund_deposit_object& obj)
               {
                  obj.enabled = false;
                  obj.finished = true;
               });

               // withdraw deposit amount
               if ( (db.head_block_time() <= HARDFORK_628_TIME)
                    || ((db.head_block_time() > HARDFORK_628_TIME) && (dep.amount.amount > 0))
                    || (db.head_block_time() > HARDFORK_634_TIME) )
               {
                  if ( ((db.head_block_time() > HARDFORK_634_TIME) && (dep.amount.amount > 0))
                       || (db.head_block_time() <= HARDFORK_634_TIME) )
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
                     } catch (fc::assert_exception& e) { }

                     // reduce fund balance
                     db.modify(*this, [&](chain::fund_object& obj)
                     {
                        obj.balance -= dep.amount.amount;

                        if (db.head_block_time() < HARDFORK_640_TIME) {
                           obj.balance_before_hf640 -= dep.amount.amount;
                        }
                     });
                  }
               }
            }
         }
      }
   });

   /***************** make payment to fund owner *****************/

   asset mlm_profit;

   // case 1
   if (payment_scheme == fund_payment_scheme::residual)
   {
      // MLM payment. In current scheme we're using percent from entire fund balance
      mlm_profit = asset(std::roundl((long double)old_balance.value * db.get_percent(mlm_daily_percent)), asset_id);

      const optional<fund_options::fund_rate>& p_rate = get_max_fund_rate(old_balance);
      if (p_rate.valid())
      {
         share_type fund_day_profit = std::roundl((long double)old_balance.value * get_rate_percent(*p_rate, db));
         if (fund_day_profit > 0)
         {
            share_type owner_profit = fund_day_profit - daily_payments_without_owner;

            h_item.daily_payments_total = fund_day_profit;
            h_item.daily_payments_owner = 0;
            h_item.daily_payments_without_owner = daily_payments_without_owner;

            const asset& owner_quantity = db.check_supply_overflow(asst.amount(owner_profit));

//             std::cout << "old_balance: " << old_balance.value
//                       << ", fund_day_profit: " << fund_day_profit.value
//                       << ", daily_payments_without_owner: " << daily_payments_without_owner.value
//                       << ", owner profit: " << owner_profit.value << std::endl;

            if (owner_quantity.amount.value > 0)
            {
               // monthly payment
               if ((db.head_block_time() >= HARDFORK_630_TIME) && owner_monthly_payments_enabled)
               {
                  // payment datetime has already reached
                  if (db.head_block_time() >= owner_monthly_payments_next_datetime)
                  {
                     const share_type& p = owner_monthly_payments_amount + owner_quantity.amount;
                     h_item.daily_payments_owner = p;

                     chain::fund_payment_operation op;
                     op.issuer = asst.issuer;
                     op.fund_id = id;
                     op.asset_to_issue = asset(p, asset_id);
                     op.issue_to_account = owner;

                     try
                     {
                        op.validate();
                        db.apply_operation(eval, op);
                     } catch (fc::assert_exception& e) { }

                     db.modify(*this, [&](chain::fund_object& obj)
                     {
                        obj.owner_monthly_payments_amount = 0;
                        obj.owner_monthly_payments_next_datetime = db.head_block_time() + fc::days(30);
                     });
                  }
                  // payment datetime has not reached yet
                  else
                  {
                     db.modify(*this, [&](chain::fund_object& obj) {
                        obj.owner_monthly_payments_amount += owner_quantity.amount;
                     });
                  }
               }
               // daily payment
               else if ((db.head_block_time() < HARDFORK_630_TIME) || !owner_monthly_payments_enabled)
               {
                  h_item.daily_payments_owner = owner_profit;

                  chain::fund_payment_operation op;
                  op.issuer = asst.issuer;
                  op.fund_id = id;
                  op.asset_to_issue = owner_quantity;
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
   }
   // case 2
   else if (payment_scheme == fund_payment_scheme::fixed)
   {
      // MLM payment. In current scheme we're using percent from only user deposits
      mlm_profit = asset(std::roundl((long double)all_deposits.value * db.get_percent(mlm_daily_percent)), asset_id);

      const optional<fund_options::fund_rate>& p_rate = get_max_fund_rate(owner_balance);
      if (p_rate)
      {
         share_type quantity;
         if (db.head_block_time() >= HARDFORK_630_TIME) {
            quantity = std::roundl(db.get_percent(p_rate->day_percent) * (long double)all_deposits.value);
         }
         else {
            quantity = std::roundl(db.get_percent(p_rate->day_percent) * (long double)daily_payments_without_owner.value);
         }

         const asset& owner_quantity = db.check_supply_overflow(asst.amount(quantity));

         if (owner_quantity.amount.value > 0)
         {
            h_item.daily_payments_total = 0;
            h_item.daily_payments_owner = 0;
            h_item.daily_payments_without_owner = daily_payments_without_owner;

            // monthly payment
            if ((db.head_block_time() >= HARDFORK_630_TIME) && owner_monthly_payments_enabled)
            {
               // payment datetime has already reached
               if (db.head_block_time() >= owner_monthly_payments_next_datetime)
               {
                  const share_type& p = owner_monthly_payments_amount + owner_quantity.amount;
                  h_item.daily_payments_total = p + daily_payments_without_owner;
                  h_item.daily_payments_owner = p;

                  chain::fund_payment_operation op;
                  op.issuer = asst.issuer;
                  op.fund_id = id;
                  op.asset_to_issue = asset(p, asset_id);
                  op.issue_to_account = owner;

                  try
                  {
                     op.validate();
                     db.apply_operation(eval, op);
                  } catch (fc::assert_exception& e) { }

                  db.modify(*this, [&](chain::fund_object& obj)
                  {
                     obj.owner_monthly_payments_amount = 0;
                     obj.owner_monthly_payments_next_datetime = db.head_block_time() + fc::days(30);
                  });
               }
               // payment datetime has not reached yet
               else
               {
                  db.modify(*this, [&](chain::fund_object& obj) {
                     obj.owner_monthly_payments_amount += owner_quantity.amount;
                  });
               }
            }
            // daily payment
            else if ((db.head_block_time() < HARDFORK_630_TIME) || !owner_monthly_payments_enabled)
            {
               h_item.daily_payments_total = owner_quantity.amount + daily_payments_without_owner;
               h_item.daily_payments_owner = owner_quantity.amount;

               chain::fund_payment_operation op;
               op.issuer = asst.issuer;
               op.fund_id = id;
               op.asset_to_issue = owner_quantity;
               op.issue_to_account = owner;

               try
               {
                  op.validate();
                  db.apply_operation(eval, op);
               } catch (fc::assert_exception& e) { }
            }
         }
      }
      else {
         wlog("Not enough owner balance to make payment to owner. Owner: ${o}, balance: ${b}", ("o", owner)("b", owner_balance));
      }
   }

   /************ additionally make payment to MLM-account ************/

   mlm_profit = db.check_supply_overflow(mlm_profit);

   if ( (db.head_block_time() >= HARDFORK_630_TIME)
        && (mlm_account != account_id_type() )
        && (mlm_profit.amount.value > 0) )
   {
      // monthly payment
      if (mlm_monthly_payments_enabled)
      {
         // payment datetime has already reached
         if (db.head_block_time() >= mlm_monthly_payments_next_datetime)
         {
            const share_type& p = mlm_monthly_payments_amount + mlm_profit.amount;

            chain::fund_payment_operation op;
            op.issuer = asst.issuer;
            op.fund_id = id;
            op.asset_to_issue = asset(p, asset_id);
            op.issue_to_account = mlm_account;

            try
            {
               op.validate();
               db.apply_operation(eval, op);
            } catch (fc::assert_exception& e) { }

            db.modify(*this, [&](chain::fund_object& obj)
            {
               obj.mlm_monthly_payments_amount = 0;
               obj.mlm_monthly_payments_next_datetime = db.head_block_time() + fc::days(30);
            });
         }
         // payment datetime has not reached yet
         else
         {
            db.modify(*this, [&](chain::fund_object& obj) {
               obj.mlm_monthly_payments_amount += mlm_profit.amount;
            });
         }
      }
      // daily payment
      else
      {
         chain::fund_payment_operation op;
         op.issuer = asst.issuer;
         op.fund_id = id;
         op.asset_to_issue = mlm_profit;
         op.issue_to_account = mlm_account;

         try
         {
            op.validate();
            db.apply_operation(eval, op);
         } catch (fc::assert_exception& e) { }
      }
   }

   // erase old history items
   if ( (h_item.daily_payments_owner > 0)
        || (h_item.daily_payments_total > 0)
        || (h_item.daily_payments_without_owner > 0) )
   {
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
   db.modify(*this, [&](chain::fund_object& obj)
   {
      if (owner_deps > 0)
      {
         obj.balance -= owner_deps;

         if (db.head_block_time() < HARDFORK_640_TIME) {
            obj.balance_before_hf640 -= owner_deps;
         }
      }
      obj.owner_balance = 0;
      obj.enabled = false;
   });
}

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fund_history_object,
                    (graphene::chain::object),
                    (owner)(items) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fund_statistics_object,
                    (graphene::chain::object),
                    (owner)
                    (most_recent_op)
                    (total_ops)
                    (deposit_count) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fund_transaction_history_object,
                    (graphene::chain::object),
                    (fund)
                    (operation_id)
                    (sequence)
                    (next)
                    (block_time) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fund_deposit_object,
                    (graphene::db::object),
                    (fund_id)
                    (account_id)
                    (amount)
                    (enabled)
                    (finished)
                    (datetime_begin)
                    (datetime_end)
                    (prev_maintenance_time_on_creation)
                    (period)
                    (percent)
                    (daily_payment)
                    (manual_percent_enabled)
                    (can_use_percent) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fund_object,
                    (graphene::db::object),
                    (name)
                    (description)
                    (owner)
                    (asset_id)
                    (balance_before_hf640)
                    (balance)
                    (owner_balance)
                    (max_limit_deposits_amount)
                    (owner_monthly_payments_enabled)
                    (owner_monthly_payments_next_datetime)
                    (owner_monthly_payments_amount)
                    (mlm_account)
                    (mlm_monthly_payments_enabled)
                    (mlm_monthly_payments_next_datetime)
                    (mlm_daily_percent)
                    (mlm_monthly_payments_amount)
                    (enabled)
                    (datetime_begin)
                    (datetime_end)
                    (prev_maintenance_time_on_creation)
                    (deposit_acceptance_enabled)
                    (rates_reduction_per_month)
                    (period)
                    (min_deposit)
                    (statistics_id)
                    (history_id)
                    (payment_scheme)
                    (payment_rates)
                    (fund_rates) )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fund_history_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fund_statistics_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fund_transaction_history_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fund_deposit_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fund_object )
