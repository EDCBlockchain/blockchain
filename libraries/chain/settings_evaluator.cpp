#include <graphene/chain/settings_evaluator.hpp>
#include <graphene/chain/settings_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <iostream>

namespace graphene { namespace chain {

void_result settings_evaluator::do_evaluate( const update_settings_operation& op )
{ try {

   database& d = db();

   auto itr = d.find(settings_id_type(0));
   FC_ASSERT(itr, "can't find settings_object'!");

   settings_ptr = &(*itr);

   FC_ASSERT(op.edc_transfers_daily_limit > 0, "edc_transfers_daily_limit must be > 0");

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result settings_evaluator::do_apply( const update_settings_operation& op )
{ try {

   database& d = db();

   d.modify(*settings_ptr, [&](settings_object& obj)
   {
      obj.transfer_fees              = op.transfer_fees;
      obj.blind_transfer_fees        = op.blind_transfer_fees;
      obj.blind_transfer_default_fee = op.blind_transfer_default_fee;
      obj.edc_deposit_max_sum        = op.edc_deposit_max_sum;
      obj.edc_transfers_daily_limit  = op.edc_transfers_daily_limit;

      if (op.extensions.value.cheque_fees) {
         obj.cheque_fees = *op.extensions.value.cheque_fees;
      }
      if (op.extensions.value.create_market_address_fee_edc) {
         obj.create_market_address_fee_edc = *op.extensions.value.create_market_address_fee_edc;
      }
      if (op.extensions.value.edc_cheques_daily_limit) {
         obj.edc_cheques_daily_limit = *op.extensions.value.edc_cheques_daily_limit;
      }
      if (op.extensions.value.block_reward) {
         obj.block_reward = *op.extensions.value.block_reward;
      }
      if (op.extensions.value.witness_fees_percent) {
         obj.witness_fees_percent = *op.extensions.value.witness_fees_percent;
      }
   });

   return void_result{};

} FC_CAPTURE_AND_RETHROW( (op) ) }


} } // graphene::chain
