#include <graphene/chain/fund_evaluator.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/settings_object.hpp>

namespace graphene { namespace chain {

void_result fund_create_evaluator::do_evaluate( const fund_create_operation& op )
{ try {

   database& d = db();

   FC_ASSERT(d.find_object(op.owner), "Account ${a} doesn't exist", ("a", op.owner));
   FC_ASSERT(d.find_object(op.asset_id), "Asset ${a} doesn't exist", ("a", op.asset_id));
   FC_ASSERT(op.name.length() > 0, "Fund name is too short!");

   check_fund_options(op.options, d, fund_payment_scheme::residual);

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_name>();
   auto itr = idx.find(op.name);
   FC_ASSERT(itr == idx.end(), "Fund with name '${a}' already exists!", ("a", op.name));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type fund_create_evaluator::do_apply( const fund_create_operation& op )
{ try {

   database& d = db();

   auto next_fund_id = d.get_index_type<fund_index>().get_next_id();

   const fund_object& new_fund =
   d.create<fund_object>([&](fund_object& o)
   {
      o.name        = op.name;
      o.description = op.options.description;
      o.owner       = op.owner;
      o.asset_id    = op.asset_id;

      if (op.extensions.size() > 0)
      {
         const protocol::fund_ext_info& ext = op.extensions.begin()->get<protocol::fund_ext_info>();

         o.max_limit_deposits_amount = ext.max_limit_deposits_amount;
         o.owner_monthly_payments_enabled = ext.owner_monthly_payments_enabled;
         if (o.owner_monthly_payments_enabled) {
            o.owner_monthly_payments_next_datetime = d.head_block_time() + fc::days(30);
         }
         o.mlm_account = ext.mlm_account;
         o.mlm_monthly_payments_enabled = ext.mlm_monthly_payments_enabled;
         if (o.mlm_monthly_payments_enabled) {
            o.mlm_monthly_payments_next_datetime = d.head_block_time() + fc::days(30);
         }
         o.mlm_daily_percent = ext.mlm_daily_percent;
      }

      o.datetime_begin                    = d.head_block_time();
      o.prev_maintenance_time_on_creation = d.get_dynamic_global_properties().last_budget_time;
      o.datetime_end                      = o.prev_maintenance_time_on_creation + (86400 * op.options.period);
      o.rates_reduction_per_month = op.options.rates_reduction_per_month;
      o.period                    = op.options.period;
      o.min_deposit               = op.options.min_deposit;
      o.payment_rates             = op.options.payment_rates;
      o.fund_rates                = op.options.fund_rates;

      // fund statistics
      o.statistics_id = db().create<fund_statistics_object>([&](fund_statistics_object& s) {s.owner = o.id;}).id;

      // fund history
      o.history_id = db().create<fund_history_object>([&](fund_history_object& s) {s.owner = o.id;}).id;

   });

   FC_ASSERT(new_fund.id == next_fund_id);

   return next_fund_id;

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_update_evaluator::do_evaluate( const fund_update_operation& op )
{ try {

   database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT( itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id) );

   fund_obj_ptr = &(*itr);
   check_fund_options(op.options, d, fund_obj_ptr->payment_scheme);

   const fund_object& fund_obj = *fund_obj_ptr;
   bool period_valid = (fund_obj.prev_maintenance_time_on_creation + (86400 * op.options.period)) >= d.get_dynamic_global_properties().next_maintenance_time;

   FC_ASSERT( period_valid, "New period '${period}' for fund '${fund}' is invalid! Fund maint-time with new period: ${a}; node next maint-time: ${b}",
                            ("period", op.options.period)
                            ("fund", fund_obj.name)
                            ("a", std::string(fund_obj.prev_maintenance_time_on_creation + (86400 * op.options.period)))
                            ("b", std::string(d.get_dynamic_global_properties().next_maintenance_time))
            );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_update_evaluator::do_apply( const fund_update_operation& op )
{ try {

   FC_ASSERT(fund_obj_ptr);

   database& d = db();

   const fund_object& fund = *fund_obj_ptr;

   d.modify(fund, [&](chain::fund_object& o)
   {
      o.description               = op.options.description;
      o.rates_reduction_per_month = op.options.rates_reduction_per_month;
      o.period                    = op.options.period;

      if (op.extensions.size() > 0)
      {
         const protocol::fund_ext_info& ext = op.extensions.begin()->get<protocol::fund_ext_info>();

         o.max_limit_deposits_amount = ext.max_limit_deposits_amount;
         o.owner_monthly_payments_enabled = ext.owner_monthly_payments_enabled;
         if (o.owner_monthly_payments_enabled) {
            o.owner_monthly_payments_next_datetime = d.head_block_time() + fc::days(30);
         }
         o.mlm_account = ext.mlm_account;
         o.mlm_monthly_payments_enabled = ext.mlm_monthly_payments_enabled;
         if (o.mlm_monthly_payments_enabled) {
            o.mlm_monthly_payments_next_datetime = d.head_block_time() + fc::days(30);
         }
         o.mlm_daily_percent = ext.mlm_daily_percent;
      }

      // so we need also update 'datetime_end'
      o.datetime_end              = fund.prev_maintenance_time_on_creation + (86400 * op.options.period);
      o.min_deposit               = op.options.min_deposit;
      o.payment_rates             = op.options.payment_rates;
      o.fund_rates                = op.options.fund_rates;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_refill_evaluator::do_evaluate( const fund_refill_operation& op )
{ try {

   database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id));

   fund_obj_ptr = &(*itr);

   const asset_object& asset_obj = fund_obj_ptr->asset_id(d);
   const account_object& from_acc = op.from_account(d);

   FC_ASSERT( from_acc.get_id() == fund_obj_ptr->owner
              , "Only owner can refill its own fund. Account id: ${acc_id}, fund owner id: ${fund_owner_id}"
              , ("acc_id", from_acc.get_id())("fund_owner_id", fund_obj_ptr->owner) );

   FC_ASSERT( itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id) );

   bool insufficient_balance = (d.get_balance( from_acc, asset_obj ).amount >= op.amount);
   FC_ASSERT( insufficient_balance,
              "Insufficient balance: ${balance}, unable to refill fund '${fund}'",
              ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj)))
              ("fund", fund_obj_ptr->name) );

   FC_ASSERT( op.amount >= fund_obj_ptr->min_deposit
              , "Minimum balance for refilling must be at least ${balance}"
              , ("balance", fund_obj_ptr->min_deposit));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_refill_evaluator::do_apply( const fund_refill_operation& op )
{ try {

   FC_ASSERT(fund_obj_ptr);

   database& d = db();
   const fund_object& fund = *fund_obj_ptr;

   asset asst(op.amount, fund.asset_id);

   // user (fund holder)
   d.adjust_balance(op.from_account, -asst);

   // fund
   d.modify(fund, [&](chain::fund_object& o)
   {
      o.balance       += op.amount;
      o.owner_balance += op.amount;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_deposit_evaluator::do_evaluate( const fund_deposit_operation& op )
{ try {

   database& d = db();
   const settings_object& settings = *d.find(settings_id_type(0));

   {
      const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
      auto itr = idx.find(op.fund_id);
      FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.fund_id));

      fund_obj_ptr = &(*itr);
   }

   {
      const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
      auto itr = idx.find(op.from_account);
      FC_ASSERT(itr != idx.end(), "There is no account with id '${id}'!", ("id", op.fund_id));
      account_ptr = &(*itr);
   }

   p_rate = fund_obj_ptr->get_payment_rate(op.period);
   FC_ASSERT(p_rate, "Wrong period (${period}) for 'fund_deposit_create'", ("period", op.period));

   const asset_object& asset_obj = fund_obj_ptr->asset_id(d);
   const account_object& from_acc = *account_ptr;

   FC_ASSERT( not_restricted_account( d, from_acc, directionality_type::payer), "account ${a} restricted by committee", ("a", from_acc.name) );

   if ((d.head_block_time() >= HARDFORK_630_TIME) && (fund_obj_ptr->max_limit_deposits_amount > 0))
   {
      share_type balance_new = fund_obj_ptr->balance + op.amount;
      FC_ASSERT( fund_obj_ptr->max_limit_deposits_amount >= balance_new,
                 "Maximum sum of deposits is exceeded. Fund: '${f}', fund balance: ${b}, max limit: ${l}",
                 ("f", fund_obj_ptr->name)
                 ("b", fund_obj_ptr->balance)
                 ("l", fund_obj_ptr->max_limit_deposits_amount) );
   }

   bool insufficient_balance = (d.get_balance( from_acc, asset_obj ).amount >= op.amount);
   FC_ASSERT( insufficient_balance,
              "Insufficient balance: ${balance}, user: {$user}, fund: '${fund}'. Unable to create deposit.",
              ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj)))
              ("user", from_acc.name)
              ("fund", fund_obj_ptr->name) );

   FC_ASSERT( op.amount >= fund_obj_ptr->min_deposit
              , "Minimum balance for deposit must be at least ${balance}"
              , ("balance", fund_obj_ptr->min_deposit));

   if ((d.head_block_time() > HARDFORK_627_TIME) && (fund_obj_ptr->asset_id == EDC_ASSET))
   {
      FC_ASSERT(settings.edc_deposit_max_sum >= (from_acc.edc_in_deposits + op.amount),
                "Maximum balance exceeded. Current sum of user deposits (all funds): ${a}. Max deposit sum: ${b}",
                ("a", from_acc.edc_in_deposits)("b", settings.edc_deposit_max_sum));
   }

   // lifetime of deposit must be less than fund's
   bool check_period = fund_obj_ptr->datetime_end >= (d.get_dynamic_global_properties().last_budget_time + op.period);
   FC_ASSERT( check_period, "Wrong period of deposit (${p}) or fund datetime_end exceeded!", ("p", op.period) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

operation_result fund_deposit_evaluator::do_apply( const fund_deposit_operation& op )
{ try {

   FC_ASSERT(fund_obj_ptr);

   eval_fund_dep_apply_object result;
   eval_fund_dep_apply_object_fixed result_new;

   database& d = db();
   const chain::fund_object& fund = *fund_obj_ptr;
   const account_object& from_acc = *account_ptr;

   // new deposit object
   auto next_fund_deposit_id = d.get_index_type<fund_deposit_index>().get_next_id();

   const fund_deposit_object& new_fund_deposit =
   d.create<fund_deposit_object>([&](fund_deposit_object& o)
   {
      o.fund_id    = op.fund_id;
      o.account_id = op.from_account;
      o.amount     = asset(op.amount, fund.asset_id);
      o.datetime_begin = d.head_block_time();

      if (d.head_block_time() > HARDFORK_625_TIME)
      {
         o.prev_maintenance_time_on_creation = d.get_dynamic_global_properties().next_maintenance_time -
                                               d.get_global_properties().parameters.maintenance_interval;
      }
      else
      {
         // last_budget_time - not stable
         o.prev_maintenance_time_on_creation = d.get_dynamic_global_properties().last_budget_time;
      }

      o.datetime_end = o.prev_maintenance_time_on_creation + (86400 * op.period);
      o.period = op.period;
      o.percent = p_rate->percent;
      o.daily_payment = d.get_deposit_daily_payment(o.percent, o.period, o.amount.amount);

      if (d.head_block_time() >= HARDFORK_626_TIME)
      {
         result_new.datetime_begin = o.datetime_begin;
         result_new.datetime_end   = o.datetime_end;
         result_new.daily_payment  = asset(o.daily_payment, fund.asset_id);
      }
      else
      {
         result.datetime_begin = o.datetime_begin;
         result.datetime_end   = o.datetime_end;
         result.percent        = o.percent;
      }
   });

   asset asst(op.amount, fund.asset_id);

   // ========= statistics

   d.modify(fund.statistics_id(d), [&](fund_statistics_object& obj)
   {
      // fund: increasing deposits count
      ++obj.deposit_count;
   });
   d.modify(from_acc, [&](account_object& obj)
   {
      auto itr = obj.deposit_sums.find(fund.get_id());
      if (itr != obj.deposit_sums.end()) {
         itr->second += asset(op.amount, fund.asset_id);
      }
      else {
         obj.deposit_sums[fund.get_id()] = asset(op.amount, fund.asset_id);
      }

      if (fund.asset_id == EDC_ASSET) {
         obj.edc_in_deposits = d.get_user_deposits_sum(op.from_account, EDC_ASSET);
      }
   });

   // ====================

   // user
   d.adjust_balance( op.from_account, -asst );

   // fund: increasing balance
   d.modify(fund, [&](chain::fund_object& o) {
      o.balance += op.amount;
   });

   FC_ASSERT(new_fund_deposit.id == next_fund_deposit_id);

   if ( (d.head_block_time() > HARDFORK_627_TIME) && (fund.asset_id == EDC_ASSET) ) {
      d.rebuild_user_edc_deposit_availability(op.from_account);
   }

   if (d.head_block_time() >= HARDFORK_626_TIME)
   {
      result_new.id = new_fund_deposit.id;
      return result_new;
   }
   else
   {
      result.id = new_fund_deposit.id;
      return result;
   }

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_withdrawal_evaluator::do_evaluate( const fund_withdrawal_operation& op )
{ try {

   const database& d = db();

   const asset_object& asst_obj = op.asset_to_issue.asset_id(d);
   FC_ASSERT( op.issuer == asst_obj.issuer );
   FC_ASSERT( !asst_obj.is_market_issued(), "Cannot manually issue a market-issued asset." );

   {
      const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
      auto itr = idx.find(op.fund_id);
      FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.fund_id));
      fund_obj_ptr = &(*itr);
   }

   {
      const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
      auto itr = idx.find(op.issue_to_account);
      FC_ASSERT(itr != idx.end(), "There is no account with id '${id}'!", ("id", op.issue_to_account));
      account_ptr = &(*itr);
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_withdrawal_evaluator::do_apply( const fund_withdrawal_operation& op )
{ try {

   database& d = db();

   d.adjust_balance(op.issue_to_account, op.asset_to_issue);

   const chain::fund_object& fund = *fund_obj_ptr;

   // update stats
   d.modify(*account_ptr, [&](account_object& obj)
   {
      auto itr = obj.deposit_sums.find(fund.get_id());
      if (itr != obj.deposit_sums.end()) {
         itr->second -= asset(op.asset_to_issue.amount, fund.asset_id);
      }

      if (fund.asset_id == EDC_ASSET) {
         obj.edc_in_deposits = d.get_user_deposits_sum(op.issue_to_account, EDC_ASSET);
      }
   });

   if ( (fund.asset_id == EDC_ASSET) && (d.head_block_time() > HARDFORK_627_TIME) ) {
      d.rebuild_user_edc_deposit_availability(op.issue_to_account);
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_payment_evaluator::do_evaluate( const fund_payment_operation& op )
{ try {

   const database& d = db();

   const asset_object& asst_obj = op.asset_to_issue.asset_id(d);
   FC_ASSERT( op.issuer == asst_obj.issuer );
   FC_ASSERT( !asst_obj.is_market_issued(), "Cannot manually issue a market-issued asset." );

   const account_object& to_account = op.issue_to_account(d);
   FC_ASSERT( is_authorized_asset( d, to_account, asst_obj), "Not authorized asset '${asst}'!", ("asst", asst_obj.symbol));
   FC_ASSERT( not_restricted_account(d, to_account, directionality_type::receiver), "Account '${acc}' is restricted!", ("acc", to_account.name) );

   asset_dyn_data_ptr = &asst_obj.dynamic_asset_data_id(d);
   FC_ASSERT( ((asset_dyn_data_ptr->current_supply + op.asset_to_issue.amount) <= asst_obj.options.max_supply)
              , "Try to issue more than max_supply! Max_supply of '${asst}': '${max_spl}'), current supply: '${cur_spl}'"
              , ("asst", asst_obj.symbol)("max_spl", asst_obj.options.max_supply)("cur_spl", asset_dyn_data_ptr->current_supply) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_payment_evaluator::do_apply( const fund_payment_operation& op )
{ try {

   database& d = db();

   d.adjust_balance(op.issue_to_account, op.asset_to_issue);

   // current supply
   if (asset_dyn_data_ptr)
   {
      d.modify(*asset_dyn_data_ptr, [&](asset_dynamic_data_object& data) {
         data.current_supply += op.asset_to_issue.amount;
      });
      asset_dyn_data_ptr = nullptr;
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_set_enable_evaluator::do_evaluate( const fund_set_enable_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id));

   fund_obj_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_set_enable_evaluator::do_apply( const fund_set_enable_operation& op )
{ try {

   FC_ASSERT(fund_obj_ptr);

   db().modify(*fund_obj_ptr, [&](fund_object& o) {
      o.enabled = op.enabled;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_deposit_set_enable_evaluator::do_evaluate( const fund_deposit_set_enable_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_deposit_index>().indices().get<by_id>();
   auto itr = idx.find(op.deposit_id);
   FC_ASSERT( itr != idx.end(), "There is no deposit with id '${id}'!", ("id", op.deposit_id) );

   fund_dep_obj_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_deposit_set_enable_evaluator::do_apply( const fund_deposit_set_enable_operation& op )
{ try {

   FC_ASSERT(fund_dep_obj_ptr);

   db().modify(*fund_dep_obj_ptr, [&](fund_deposit_object& o) {
      o.enabled = op.enabled;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_remove_evaluator::do_evaluate( const fund_remove_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT( itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id) );

   fund_obj_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_remove_evaluator::do_apply( const fund_remove_operation& op )
{ try {

     (void)op;
//   FC_ASSERT(fund_obj_ptr);
//
//   database& d = db();
//
//   const chain::fund_object& fund = *fund_obj_ptr;
//
//   // remove fund statistics
//   const fund_statistics_object* fund_stat_ptr = d.find(fc::variant(fund.get_statistics_id(), 1).as<fund_statistics_id_type>(1));
//   if (fund_stat_ptr) {
//      d.remove(*fund_stat_ptr);
//   }
//
//   // remove history object
//   const fund_history_object* fund_hist_ptr = d.find(fc::variant(fund.get_history_id(), 1).as<fund_history_id_type>(1));
//   if (fund_hist_ptr) {
//      d.remove(*fund_hist_ptr);
//   }
//
//   // remove fund
//   d.remove(fund);
//
//   // remove fund deposits
//   auto range = db().get_index_type<fund_deposit_index>().indices().get<by_fund_id>().equal_range(op.id);
//   std::for_each(range.first, range.second, [&](const fund_deposit_object& dep) {
//      d.remove(dep);
//   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_change_payment_scheme_evaluator::do_evaluate( const fund_change_payment_scheme_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT( itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.id) );

   fund_obj_ptr = &(*itr);

   FC_ASSERT( op.payment_scheme <= fund_payment_scheme::fixed );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_change_payment_scheme_evaluator::do_apply( const fund_change_payment_scheme_operation& op )
{ try {

   FC_ASSERT(fund_obj_ptr);

   database& d = db();

   d.modify(*fund_obj_ptr, [&](chain::fund_object& o) {
      o.payment_scheme = static_cast<fund_payment_scheme>(op.payment_scheme);
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result enable_autorenewal_deposits_evaluator::do_evaluate( const enable_autorenewal_deposits_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(op.account_id);
   FC_ASSERT( itr != idx.end(), "There is no account with id '${id}'!", ("id", op.account_id) );

   account_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result enable_autorenewal_deposits_evaluator::do_apply( const enable_autorenewal_deposits_operation& op )
{ try {

   FC_ASSERT(account_ptr);

   database& d = db();

   d.modify(*account_ptr, [&](account_object& o) {
      o.deposits_autorenewal_enabled = op.enabled;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result deposit_renewal_evaluator::do_evaluate( const deposit_renewal_operation& op )
{ try {

   const database& d = db();

   auto itr = d.find(op.deposit_id);
   FC_ASSERT(itr, "deposit '${dep}' not found!", ("dep", op.deposit_id));

   fund_deposit_ptr = &(*itr);

   return void_result();

}  FC_CAPTURE_AND_RETHROW( (op) ) }

operation_result deposit_renewal_evaluator::do_apply( const deposit_renewal_operation& o )
{ try {
   database& d = db();

   asset result;

   if (fund_deposit_ptr)
   {
      d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
      {
         dep.percent = o.percent;
         dep.datetime_end = o.datetime_end;

         if (d.head_block_time() > HARDFORK_627_TIME)
         {
            dep.daily_payment = d.get_deposit_daily_payment(dep.percent, fund_deposit_ptr->period, fund_deposit_ptr->amount.amount);
            result = asset(dep.daily_payment, dep.amount.asset_id);
         }
         else if (d.head_block_time() >= HARDFORK_626_TIME) {
            dep.daily_payment = d.get_deposit_daily_payment(fund_deposit_ptr->percent, fund_deposit_ptr->period, fund_deposit_ptr->amount.amount);
         }
      });

      if ( (d.head_block_time() > HARDFORK_627_TIME) && (fund_deposit_ptr->amount.asset_id == EDC_ASSET) ) {
         d.rebuild_user_edc_deposit_availability(o.account_id);
      }
   }

   if (d.head_block_time() > HARDFORK_628_TIME) {
      return result;
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result fund_deposit_update_evaluator::do_evaluate( const fund_deposit_update_operation& op )
{ try {

   const database& d = db();

   {
      auto itr = d.find(op.deposit_id);
      FC_ASSERT(itr, "deposit '${dep}' not found!", ("dep", op.deposit_id));
      fund_deposit_ptr = &(*itr);
   }

   {
      const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
      auto itr = idx.find(fund_deposit_ptr->fund_id);
      FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.deposit_id));
      fund_obj_ptr = &(*itr);
   }

   if (op.extensions.value.account_id)
   {
      auto itr = d.find(*op.extensions.value.account_id);
      FC_ASSERT(itr, "account '${a}' not found!", ("a", op.extensions.value.account_id));
      FC_ASSERT(fund_deposit_ptr->account_id != *op.extensions.value.account_id
                , "accounts must be different! Old account ID '${a}', new account ID '${b}'"
                , ("a", fund_deposit_ptr->account_id)("b", op.extensions.value.account_id));
   }

   if (op.extensions.value.datetime_end)
   {
      FC_ASSERT(*op.extensions.value.datetime_end > (d.head_block_time() + fc::seconds(60)),
                "'datetime_end' must be greater than 'd.head_block_time() + 1 min'. Datetime_end ${a}. Head_block_time ${b}",
                ("a", op.extensions.value.datetime_end)
                ("b", d.head_block_time()) );

      // lifetime of deposit must be less than fund's
      bool check_period = fund_obj_ptr->datetime_end >= *op.extensions.value.datetime_end;
      FC_ASSERT( check_period, "Wrong 'datetime_end' of deposit (${a})!", ("a", *op.extensions.value.datetime_end) );
   }

   if (op.reset)
   {
      p_rate = fund_obj_ptr->get_payment_rate(fund_deposit_ptr->period);
      FC_ASSERT(p_rate, "Wrong period (${period}) for 'fund_deposit_create'", ("period", fund_deposit_ptr->period));
   }

   return void_result();

}  FC_CAPTURE_AND_RETHROW( (op) ) }

dep_update_info fund_deposit_update_evaluator::do_apply( const fund_deposit_update_operation& o )
{ try {
   database& d = db();
   const fund_object& fund = *fund_obj_ptr;
   const fund_deposit_object& fund_deposit = *fund_deposit_ptr;

   // change deposit owner
   if (o.extensions.value.account_id)
   {
      account_id_type old_account_id = fund_deposit.account_id;
      account_id_type new_account_id = *o.extensions.value.account_id;

      d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep) {
         dep.account_id = new_account_id;
      });

      // ========= statistics

      d.modify(old_account_id(d), [&](account_object& obj)
      {
         auto itr = obj.deposit_sums.find(fund.get_id());
         if (itr != obj.deposit_sums.end()) {
            itr->second -= asset(fund_deposit.amount.amount, fund.asset_id);
         }
      });
      d.modify(new_account_id(d), [&](account_object& obj)
      {
         auto itr = obj.deposit_sums.find(fund.get_id());
         if (itr != obj.deposit_sums.end()) {
            itr->second += asset(fund_deposit.amount.amount, fund.asset_id);
         }
         else {
            obj.deposit_sums[fund.get_id()] = asset(fund_deposit.amount.amount, fund.asset_id);
         }
      });

      if (fund.asset_id == EDC_ASSET)
      {
         d.modify(old_account_id(d), [&](account_object& obj) {
            obj.edc_in_deposits = d.get_user_deposits_sum(old_account_id, EDC_ASSET);
         });
         d.modify(new_account_id(d), [&](account_object& obj) {
            obj.edc_in_deposits = d.get_user_deposits_sum(new_account_id, EDC_ASSET);
         });
      }
   }

   // change 'datetime_end' of deposit
   if (o.extensions.value.datetime_end)
   {
      d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep) {
         dep.datetime_end = *o.extensions.value.datetime_end;
      });
   }

   // main flags
   if (!o.extensions.value.apply_extension_flags_only)
   {
      // fund percent
      if (o.reset)
      {
         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
         {
            dep.percent = p_rate->percent;
            dep.manual_percent_enabled = false;
            dep.daily_payment = d.get_deposit_daily_payment(dep.percent, dep.period, dep.amount.amount);
         });
      }
      // manual percent
      else
      {
         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
         {
            dep.percent = o.percent;
            dep.manual_percent_enabled = true;
            dep.daily_payment = d.get_deposit_daily_payment(dep.percent, dep.period, dep.amount.amount);
         });
      }
   }

   dep_update_info result;
   result.percent = fund_deposit_ptr->percent;
   result.daily_payment = asset(fund_deposit_ptr->daily_payment, fund_deposit_ptr->amount.asset_id);

   return result;

} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result fund_deposit_update2_evaluator::do_evaluate( const fund_deposit_update2_operation& op )
{ try {

   const database& d = db();

   {
      auto itr = d.find(op.deposit_id);
      FC_ASSERT(itr, "deposit '${dep}' not found!", ("dep", op.deposit_id));
      fund_deposit_ptr = &(*itr);
   }

   if (op.extensions.size() > 0)
   {
      const protocol::fund_dep_upd_ext_info& ext = op.extensions.begin()->get<protocol::fund_dep_upd_ext_info>();

      {
         const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
         auto itr = idx.find(fund_deposit_ptr->fund_id);
         FC_ASSERT(itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.deposit_id));
         fund_obj_ptr = &(*itr);
      }

      if (ext.account_id != account_id_type())
      {
         auto itr = d.find(ext.account_id);
         FC_ASSERT(itr, "account '${a}' not found!", ("a", ext.account_id));
         FC_ASSERT(fund_deposit_ptr->account_id != ext.account_id
         , "accounts must be different! Old account ID '${a}', new account ID '${b}'"
         , ("a", fund_deposit_ptr->account_id)("b", ext.account_id));
      }

      if (ext.datetime_end.sec_since_epoch() > 0)
      {
         FC_ASSERT(ext.datetime_end > (d.head_block_time() + fc::seconds(60)),
                   "'datetime_end' must be greater than 'd.head_block_time() + 1 min'. Datetime_end ${a}. Head_block_time ${b}",
                   ("a", ext.datetime_end)
                   ("b", d.head_block_time()) );

         // lifetime of deposit must be less than fund's
         bool check_period = fund_obj_ptr->datetime_end >= ext.datetime_end;
         FC_ASSERT( check_period, "Wrong 'datetime_end' of deposit (${a})!", ("a", ext.datetime_end) );
      }
   }

   if (op.reset)
   {
      p_rate = fund_obj_ptr->get_payment_rate(fund_deposit_ptr->period);
      FC_ASSERT(p_rate, "Wrong period (${period}) for 'fund_deposit_create'", ("period", fund_deposit_ptr->period));
   }

   return void_result();

}  FC_CAPTURE_AND_RETHROW( (op) ) }

dep_update_info fund_deposit_update2_evaluator::do_apply( const fund_deposit_update2_operation& o )
{ try {
   database& d = db();
   const fund_object& fund = *fund_obj_ptr;
   const fund_deposit_object& fund_deposit = *fund_deposit_ptr;

   optional<protocol::fund_dep_upd_ext_info> ext;

   if (o.extensions.size() > 0)
   {
      ext = o.extensions.begin()->get<protocol::fund_dep_upd_ext_info>();

      // change deposit owner
      if (ext->account_id != account_id_type())
      {
         account_id_type old_account_id = fund_deposit.account_id;
         account_id_type new_account_id = ext->account_id;

         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep) {
            dep.account_id = new_account_id;
         });

         // ========= statistics

         d.modify(old_account_id(d), [&](account_object& obj)
         {
            auto itr = obj.deposit_sums.find(fund.get_id());
            if (itr != obj.deposit_sums.end()) {
               itr->second -= asset(fund_deposit.amount.amount, fund.asset_id);
            }
         });
         d.modify(new_account_id(d), [&](account_object& obj)
         {
            auto itr = obj.deposit_sums.find(fund.get_id());
            if (itr != obj.deposit_sums.end()) {
               itr->second += asset(fund_deposit.amount.amount, fund.asset_id);
            }
            else {
               obj.deposit_sums[fund.get_id()] = asset(fund_deposit.amount.amount, fund.asset_id);
            }
         });

         if (fund.asset_id == EDC_ASSET)
         {
            d.modify(old_account_id(d), [&](account_object& obj) {
               obj.edc_in_deposits = d.get_user_deposits_sum(old_account_id, EDC_ASSET);
            });
            d.modify(new_account_id(d), [&](account_object& obj) {
               obj.edc_in_deposits = d.get_user_deposits_sum(new_account_id, EDC_ASSET);
            });
         }
      }

      // change 'datetime_end' of deposit
      if (ext->datetime_end.sec_since_epoch() > 0)
      {
         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep) {
            dep.datetime_end = ext->datetime_end;
         });
      }
   }

   // main flags
   if ( (o.extensions.size() == 0) || (ext && !ext->apply_extension_flags_only) )
   {
      // fund percent
      if (o.reset)
      {
         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
         {
            dep.percent = p_rate->percent;
            dep.manual_percent_enabled = false;
            dep.daily_payment = d.get_deposit_daily_payment(dep.percent, dep.period, dep.amount.amount);
         });
      }
      // manual percent
      else
      {
         d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
         {
            dep.percent = o.percent;
            dep.manual_percent_enabled = true;
            dep.daily_payment = d.get_deposit_daily_payment(dep.percent, dep.period, dep.amount.amount);
         });
      }
   }

   dep_update_info result;
   result.percent = fund_deposit_ptr->percent;
   result.daily_payment = asset(fund_deposit_ptr->daily_payment, fund_deposit_ptr->amount.asset_id);

   return result;

} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result fund_deposit_reduce_evaluator::do_evaluate( const fund_deposit_reduce_operation& op )
{ try {

   const database& d = db();

   {
      auto itr = d.find(op.deposit_id);
      FC_ASSERT(itr, "deposit '${dep}' not found!", ("dep", op.deposit_id));
      fund_deposit_ptr = &(*itr);

      FC_ASSERT(op.amount > 0, "op.amount must be greater than 0!");
      FC_ASSERT(fund_deposit_ptr->enabled, "deposit must be enabled!");
      FC_ASSERT(fund_deposit_ptr->amount.amount >= op.amount,
                "deposit amount must be greater than op.amount. Deposit amount '${a}'. op.amount '${b}'",
                ("a", fund_deposit_ptr->amount.amount)
                ("b", op.amount));
   }

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(fund_deposit_ptr->fund_id);
   FC_ASSERT( itr != idx.end(), "There is no fund with id '${id}'!", ("id", op.deposit_id) );
   fund_obj_ptr = &(*itr);

   asset_dyn_data_ptr = &fund_deposit_ptr->amount.asset_id(d).dynamic_asset_data_id(d);

   return void_result();

}  FC_CAPTURE_AND_RETHROW( (op) ) }

asset fund_deposit_reduce_evaluator::do_apply( const fund_deposit_reduce_operation& o )
{ try {

   asset result;

   database& d = db();
   const fund_object& fund = *fund_obj_ptr;
   const fund_deposit_object& fund_deposit = *fund_deposit_ptr;

   // deposit
   d.modify(*fund_deposit_ptr, [&](fund_deposit_object& dep)
   {
      dep.amount.amount -= o.amount;
      dep.daily_payment = d.get_deposit_daily_payment(dep.percent, dep.period, dep.amount.amount);
      result = dep.amount;
   });
   // fund balance
   d.modify(fund, [&](fund_object& obj) {
      obj.balance -= o.amount;
   });
   // current supply of the fund's asset
   d.modify(*asset_dyn_data_ptr, [&](asset_dynamic_data_object& data)
   {
      data.current_supply -= o.amount;
      data.fee_burnt += o.amount;
   });

   // ========= statistics

   d.modify(fund_deposit.account_id(d), [&](account_object& obj)
   {
      auto itr = obj.deposit_sums.find(fund.get_id());
      if (itr != obj.deposit_sums.end()) {
         itr->second -= asset(o.amount, fund.asset_id);
      }
   });
   if (fund.asset_id == EDC_ASSET)
   {
      d.modify(fund_deposit.account_id(d), [&](account_object& obj) {
         obj.edc_in_deposits = d.get_user_deposits_sum(fund_deposit.account_id, EDC_ASSET);
      });
   }

  return result;

} FC_CAPTURE_AND_RETHROW( (o) ) }


} } // graphene::chain
