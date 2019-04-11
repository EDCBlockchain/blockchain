#include <fc/smart_ref_impl.hpp>
#include <graphene/chain/fund_evaluator.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <iostream>

namespace graphene { namespace chain {

void_result fund_create_evaluator::do_evaluate( const fund_create_operation& op )
{ try {

   database& d = db();

   FC_ASSERT(d.find_object(op.owner), "Account ${a} doesn't exist", ("a", op.owner));
   FC_ASSERT(d.find_object(op.asset_id), "Asset ${a} doesn't exist", ("a", op.asset_id));
   FC_ASSERT(op.name.length() > 0, "Fund name is too short!");

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
   FC_ASSERT( itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id) );

   const fund_object& fund_obj = *itr;
   bool period_valid = (fund_obj.prev_maintenance_time_on_creation + (86400 * op.options.period)) >= d.get_dynamic_global_properties().next_maintenance_time;
   FC_ASSERT( period_valid, "New period '${period}' for fund '${fund}' is invalid!", ("period", op.options.period)("fund", fund_obj.get_name()) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_update_evaluator::do_apply( const fund_update_operation& op )
{ try {

   database& d = db();

   auto fund = d.get_index_type<fund_index>().indices().get<by_id>().find(op.id);

   d.modify(*fund, [&](chain::fund_object& o)
   {
      o.description               = op.options.description;
      o.rates_reduction_per_month = op.options.rates_reduction_per_month;
      o.period                    = op.options.period;
      // so we need also update 'datetime_end'
      o.datetime_end              = fund->get_prev_maintenance_time_on_creation() + (86400 * op.options.period);
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
   FC_ASSERT(itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id));

   const fund_object& fund_obj = *itr;

   const asset_object& asset_obj = fund_obj.asset_id(d);
   const account_object& from_acc = op.from_account(d);

   FC_ASSERT( from_acc.get_id() == fund_obj.owner
              , "Only owner can refill its own fund. Account id: ${acc_id}, fund owner id: ${fund_owner_id}"
              , ("acc_id", from_acc.get_id())("fund_owner_id", fund_obj.owner) );

   FC_ASSERT( itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id) );

   bool insufficient_balance = (d.get_balance( from_acc, asset_obj ).amount >= op.amount);
   FC_ASSERT( insufficient_balance,
              "Insufficient balance: ${balance}, unable to refill fund '${fund}'",
              ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj)))
              ("fund", fund_obj.get_name()) );

   FC_ASSERT( op.amount >= fund_obj.get_min_deposit(), "Minimum balance for refilling must be at least ${balance}", ("balance", fund_obj.get_min_deposit()));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_refill_evaluator::do_apply( const fund_refill_operation& op )
{ try {

   database& d = db();
   auto fund = d.get_index_type<fund_index>().indices().get<by_id>().find(op.id);

   asset asst(op.amount, fund->get_asset_id());

   // user (fund holder)
   d.adjust_balance(op.from_account, -asst);

   // fund
   d.modify(*fund, [&](chain::fund_object& o)
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

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT(itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id));

   const fund_object& fund_obj = *itr;

   auto iter = std::find_if(fund_obj.payment_rates.begin(), fund_obj.payment_rates.end(), [&op](const fund_options::payment_rate& item) {
      return (op.period == item.period);
   });
   FC_ASSERT(iter != fund_obj.payment_rates.end(), "Wrong period (${period}) for 'fund_deposit_create'", ("period", op.period));

   const asset_object& asset_obj = fund_obj.asset_id(d);
   const account_object& from_acc = op.from_account(d);

   FC_ASSERT( not_restricted_account( d, from_acc, directionality_type::payer), "account ${a} restricted by committee", ("a", from_acc.name) );

   bool insufficient_balance = (d.get_balance( from_acc, asset_obj ).amount >= op.amount);
   FC_ASSERT( insufficient_balance,
              "Insufficient balance: ${balance}, user: {$user}, fund: '${fund}'. Unable to create deposit.",
              ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj)))
              ("user", from_acc.name)
              ("fund", fund_obj.get_name()) );

   FC_ASSERT( op.amount >= fund_obj.get_min_deposit(), "Minimum balance for deposit must be at least ${balance}", ("balance", fund_obj.get_min_deposit()));

   // lifetime of deposit must be less than fund's
   bool check_period = fund_obj.get_datetime_end() >= (d.get_dynamic_global_properties().last_budget_time + op.period);
   FC_ASSERT( check_period, "Wrong period of deposit (${p})!", ("p", op.period) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type fund_deposit_evaluator::do_apply( const fund_deposit_operation& op )
{ try {

   database& d = db();
   auto fund = d.get_index_type<fund_index>().indices().get<by_id>().find(op.id);

   // new deposit object
   auto next_fund_deposit_id = d.get_index_type<fund_deposit_index>().get_next_id();

   const fund_deposit_object& new_fund_deposit =
   d.create<fund_deposit_object>([&]( fund_deposit_object& o)
   {
      o.fund_id    = op.id;
      o.account_id = op.from_account;
      o.amount     = op.amount;
      o.datetime_begin                    = d.head_block_time();
      o.prev_maintenance_time_on_creation = d.get_dynamic_global_properties().last_budget_time;
      o.datetime_end                      = o.prev_maintenance_time_on_creation + (86400 * op.period);
      o.period = op.period;
   });

   asset asst(op.amount, fund->get_asset_id());

   // fund statistics
   const auto& stats_obj = op.id(d).statistics_id(d);
   auto iter = stats_obj.users_deposits.find(op.from_account);
   if (iter != stats_obj.users_deposits.end())
   {
      d.modify(stats_obj, [&](fund_statistics_object& obj) {
         obj.users_deposits[op.from_account] += op.amount;
      });
   }
   else
   {
      d.modify(stats_obj, [&](fund_statistics_object& obj) {
         obj.users_deposits[op.from_account] = op.amount;
      });
   }
   // fund: increasing deposit count
   d.modify(stats_obj, [&](fund_statistics_object& obj) {
      ++obj.deposit_count;
   });

   // user
   d.adjust_balance( op.from_account, -asst );

   // fund: increasing balance
   d.modify(*fund, [&](chain::fund_object& o) {
      o.balance += op.amount;
   });

   FC_ASSERT(new_fund_deposit.id == next_fund_deposit_id);

   return next_fund_deposit_id;

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_withdrawal_evaluator::do_evaluate( const fund_withdrawal_operation& op )
{ try {

   const database& d = db();

   const asset_object& asst_obj = op.asset_to_issue.asset_id(d);
   FC_ASSERT( op.issuer == asst_obj.issuer );
   FC_ASSERT( !asst_obj.is_market_issued(), "Cannot manually issue a market-issued asset." );

   asset_dyn_data_ptr = &asst_obj.dynamic_asset_data_id(d);
   FC_ASSERT( ((asset_dyn_data_ptr->current_supply + op.asset_to_issue.amount) <= asst_obj.options.max_supply)
               , "Try to issue more than max_supply! Max_supply of '${asst}': '${max_spl}'), current supply: '${cur_spl}'"
               , ("asst", asst_obj.symbol)("max_spl", asst_obj.options.max_supply)("cur_spl", asset_dyn_data_ptr->current_supply) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_withdrawal_evaluator::do_apply( const fund_withdrawal_operation& op )
{ try {

   database& d = db();

   d.adjust_balance(op.issue_to_account, op.asset_to_issue);

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
   FC_ASSERT(itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id));

   fund_obj_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_set_enable_evaluator::do_apply( const fund_set_enable_operation& op )
{ try {

   if (fund_obj_ptr)
   {
      db().modify(*fund_obj_ptr, [&](fund_object& o) {
         o.enabled = op.enabled;
      });
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_deposit_set_enable_evaluator::do_evaluate( const fund_deposit_set_enable_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_deposit_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT( itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id) );

   fund_dep_obj_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_deposit_set_enable_evaluator::do_apply( const fund_deposit_set_enable_operation& op )
{ try {

   if (fund_dep_obj_ptr)
   {
      db().modify(*fund_dep_obj_ptr, [&](fund_deposit_object& o) {
         o.enabled = op.enabled;
      });
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

////////////////////////////////////////////////////////////////

void_result fund_remove_evaluator::do_evaluate( const fund_remove_operation& op )
{ try {

   const database& d = db();

   const auto& idx = d.get_index_type<fund_index>().indices().get<by_id>();
   auto itr = idx.find(op.id);
   FC_ASSERT( itr != idx.end(), "Where is no fund with id '${id}'!", ("id", op.id) );
   const fund_object& fund = *itr;

   const fund_statistics_object* fund_stat_ptr = d.find(fc::variant(fund.get_statistics_id()).as<fund_statistics_id_type>());
   FC_ASSERT( fund_stat_ptr, "Where is no fund statistics with id '${id}'!", ("id", fund.get_statistics_id()) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result fund_remove_evaluator::do_apply( const fund_remove_operation& op )
{ try {

   database& d = db();

   const chain::fund_object& fund = d.get<chain::fund_object>(op.id);

   // remove fund statistics
   const chain::fund_statistics_object& fund_stat = d.get<chain::fund_statistics_object>(fund.get_statistics_id());
   d.remove(fund_stat);

   // remove history object
   const chain::fund_history_object& history_obj = d.get<chain::fund_history_object>(fund.get_history_id());
   d.remove(history_obj);

   // remove fund
   d.remove(fund);

   // remove fund deposits
   auto range = db().get_index_type<fund_deposit_index>().indices().get<by_fund_id>().equal_range(op.id);
   std::for_each(range.first, range.second, [&](const fund_deposit_object& dep) {
      d.remove(dep);
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
