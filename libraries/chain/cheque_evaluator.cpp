#include <fc/smart_ref_impl.hpp>
#include <graphene/chain/cheque_evaluator.hpp>
#include <graphene/chain/cheque_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/settings_object.hpp>

namespace graphene { namespace chain {

   void_result cheque_create_evaluator::do_evaluate( const cheque_create_operation& op )
   { try {

      database& d = db();

      FC_ASSERT(d.find_object(op.account_id), "Account ${a} doesn't exist", ("a", op.account_id));
      FC_ASSERT(d.find_object(op.payee_amount.asset_id), "Asset ${a} doesn't exist", ("a", op.payee_amount.asset_id));

      const account_object& from_account = op.account_id(d);
      const asset_object& asset_type = op.payee_amount.asset_id(d);
      asset_dyn_data_ptr = &asset_type.dynamic_asset_data_id(d);
      const settings_object& settings = *d.find(settings_id_type(0));
      cheque_amount = op.payee_amount.amount * op.payee_count;

      const auto& idx = d.get_index_type<cheque_index>().indices().get<by_code>();
      auto itr = idx.find(op.code);
      FC_ASSERT(itr == idx.end(), "Cheque with this code is already exists!");

      FC_ASSERT(op.expiration_datetime > d.head_block_time()
                , "Invalid 'expiration_datetime': ${a}. Head block time: ${b}"
                , ("a", op.expiration_datetime)("b", d.head_block_time()));

      const account_object& from_acc = op.account_id(d);
      const asset_object& asset_obj  = op.payee_amount.asset_id(d);

      int64_t fee_percent = 0;

      if (d.head_block_time() > HARDFORK_627_TIME)
      {
         auto itr = std::find_if(settings.cheque_fees.begin(), settings.cheque_fees.end(), [&asset_type](const chain::settings_fee& f) {
            return (f.asset_id == asset_type.get_id());
         });
         if (itr != settings.cheque_fees.end())
         {
            fee_percent = itr->percent;
            custom_fee = std::round(cheque_amount.value * d.get_percent(fee_percent));

            bool balance_is_valid = d.get_balance(from_account, asset_type).amount >= (cheque_amount + custom_fee);
            FC_ASSERT(balance_is_valid,
                      "Insufficient Balance: ${balance}, unable to build cheque with amount '${amount}'. Custom fee: ${fee}",
                      ("balance", d.to_pretty_string(d.get_balance(from_account, asset_type)))
                      ("amount", d.to_pretty_string(op.payee_amount))
                      ("fee", d.to_pretty_string(asset(custom_fee, op.payee_amount.asset_id))));
         }

         if (custom_fee > 0) {
            FC_ASSERT(op.fee.amount >= custom_fee, "Wrong fee amount (${fee}) sent.", ("fee", op.fee.amount));
         }
      }

      if (fee_percent == 0)
      {
         bool insufficient_balance = (d.get_balance(from_acc, asset_obj).amount >= cheque_amount);
         FC_ASSERT(insufficient_balance,
                   "Insufficient balance=${balance}, amount=${amount}, payee_count=${pc}. Unable to create receipt.",
                   ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj)))("amount", d.to_pretty_string(
                   op.payee_amount))("pc", op.payee_count));
      }

      return void_result();

   } FC_CAPTURE_AND_RETHROW( (op) ) }

   object_id_type cheque_create_evaluator::do_apply( const cheque_create_operation& op )
   { try {

      database& d = db();

      asset asst(cheque_amount, op.payee_amount.asset_id);
      d.adjust_balance(op.account_id, -asst);

      auto next_cheque_id = d.get_index_type<cheque_index>().get_next_id();

      const cheque_object& new_cheque =
      d.create<cheque_object>([&](cheque_object& o)
      {
         o.drawer  = op.account_id;
         o.asset_id = op.payee_amount.asset_id;
         o.datetime_creation   = d.head_block_time();
         o.datetime_expiration = op.expiration_datetime;
         o.code   = op.code;
         o.status = cheque_status::cheque_new;
         o.amount_payee = op.payee_amount.amount;
         o.amount_remaining = o.amount_payee * op.payee_count;
         o.allocate_payees(op.payee_count);
      });

      FC_ASSERT(new_cheque.id == next_cheque_id);

      return next_cheque_id;

   } FC_CAPTURE_AND_RETHROW( (op) ) }

///////////////////////////////////////////////////////////////////////////

void_result cheque_use_evaluator::do_evaluate( const cheque_use_operation& op )
{ try {

   database& d = db();

   FC_ASSERT(d.find_object(op.account_id), "Account ${a} doesn't exist", ("a", op.account_id));

   const auto& idx = d.get_index_type<cheque_index>().indices().get<by_code>();
   auto itr = idx.find(op.code);
   FC_ASSERT(itr != idx.end(), "There is no cheque with code '${id}'!", ("id", op.code) );

   cheque_obj_ptr = &(*itr);

   if (d.head_block_time() >= HARDFORK_626_TIME) {
      FC_ASSERT(cheque_obj_ptr->datetime_expiration > d.head_block_time(), "Cheque is already expired");
   }

   FC_ASSERT((cheque_obj_ptr->status == cheque_status::cheque_new), "Cheque code '${code}' has been already used", ("rcode", op.code));
   FC_ASSERT((op.amount.amount == cheque_obj_ptr->amount_payee), "Cheque amount is invalid!");
   FC_ASSERT((op.amount.asset_id == cheque_obj_ptr->asset_id), "Cheque asset id is invalid!");

   for (const cheque_object::payee_item& item: cheque_obj_ptr->payees)
   {
      FC_ASSERT((item.payee != op.account_id)
                , "Cheque code '${code}' has been already used for account '${account}'", ("rcode", op.code)("account", op.account_id));
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type cheque_use_evaluator::do_apply( const cheque_use_operation& op )
{ try {

   FC_ASSERT(cheque_obj_ptr);

   database& d = db();

   const cheque_object& cheque = *cheque_obj_ptr;

   d.modify(cheque, [&](chain::cheque_object& o) {
      o.process_payee(op.account_id, d);
   });

   return cheque.get_id();

} FC_CAPTURE_AND_RETHROW( (op) ) }

///////////////////////////////////////////////////////////////////////////

void_result cheque_reverse_evaluator::do_evaluate( const cheque_reverse_operation& op )
{ try {

   database& d = db();
   const auto& idx = d.get_index_type<cheque_index>().indices().get<by_id>();
   auto itr = idx.find(op.cheque_id);
   FC_ASSERT(itr != idx.end(), "There is no cheque with ID '${id}'!", ("id", op.cheque_id));

   cheque_obj_ptr = &(*itr);

   FC_ASSERT(cheque_obj_ptr->status == cheque_status::cheque_new
             , "Incorrect cheque status for reversing (current status: '${a}')!", ("a", cheque_obj_ptr->status));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result cheque_reverse_evaluator::do_apply( const cheque_reverse_operation& op )
{ try {

   FC_ASSERT(cheque_obj_ptr);

   database& d = db();

   const cheque_object& obj = *cheque_obj_ptr;

   // collect unused amounts from payees array
   std::vector<cheque_object::payee_item> payees = cheque_obj_ptr->payees;

   for (cheque_object::payee_item& item: payees)
   {
      if (item.status == cheque_status::cheque_new)
      {
         // set all payees to drawer id
         item.payee = obj.drawer;
         item.datetime_used = d.head_block_time();
         item.status = cheque_status::cheque_used;
      }
   }

   // return amount to the owner balance
   if (obj.amount_remaining > 0) {
      d.adjust_balance(obj.drawer, asset(obj.amount_remaining, obj.asset_id));
   }

   d.modify(obj, [&](chain::cheque_object& o)
   {
      o.datetime_used  = d.head_block_time();
      o.status = cheque_status::cheque_undo;
      o.payees = payees;
      o.amount_remaining = 0;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain

