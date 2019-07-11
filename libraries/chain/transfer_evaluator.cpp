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
#include <graphene/chain/transfer_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result transfer_evaluator::do_evaluate( const transfer_operation& op )
{ try {
   
   const database& d = db();

   const account_object& from_account = op.from(d);
   to_account_ptr = &op.to(d);

   const asset_object& asset_type = op.amount.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",op.from)
         ("asset",op.amount.asset_id)
      );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, *to_account_ptr, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",op.to)
         ("asset",op.amount.asset_id)
      );
      GRAPHENE_ASSERT(
         not_restricted_account( d, from_account, directionality_type::payer),
         transfer_from_account_restricted,
         "'from' account ${from} is restricted by committee",
         ("from",op.from)
      );
      GRAPHENE_ASSERT(
         not_restricted_account( d, *to_account_ptr, directionality_type::receiver),
         transfer_to_account_restricted,
         "'to' account ${to} is restricted by committee",
         ("to",op.to)
      );

      if (asset_type.is_transfer_restricted())
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account_ptr->id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", op.amount.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= op.amount.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a", from_account.name)("t",to_account_ptr->name)
                 ("total_transfer", d.to_pretty_string(op.amount))
                 ("balance", d.to_pretty_string(d.get_balance(from_account, asset_type)))
               );

      // see also 'asset_reserve_operation'
      if (to_account_ptr->burning_mode_enabled)
      {
         FC_ASSERT(!asset_type.is_market_issued(), "Cannot reserve (burn) ${sym} because it is a market-issued asset", ("sym", asset_type.symbol));

         asset_dyn_data_ptr = &asset_type.dynamic_asset_data_id(d);
         FC_ASSERT( (asset_dyn_data_ptr->current_supply - op.amount.amount) >= 0 );
      }

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

}  FC_CAPTURE_AND_RETHROW( (op) ) }

void_result transfer_evaluator::do_apply( const transfer_operation& o )
{ try {
   database& d = db();

   d.adjust_balance(o.from, -o.amount);

   if (to_account_ptr)
   {
      // normal accrual
      if (!to_account_ptr->burning_mode_enabled) {
         d.adjust_balance(o.to, o.amount);
      }
      // burning
      else if (asset_dyn_data_ptr)
      {
         d.modify(*asset_dyn_data_ptr, [&]( asset_dynamic_data_object& data) {
            data.current_supply -= o.amount.amount;
         });
      }
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result update_blind_transfer2_settings_evaluator::do_evaluate( const update_blind_transfer2_settings_operation& op )
{ try {

   const database& d = db();

   auto itr = d.find(blind_transfer2_settings_id_type(0));

   FC_ASSERT(itr, "can't find 'blind_transfer2_settings_object'!");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_blind_transfer2_settings_evaluator::do_apply( const update_blind_transfer2_settings_operation& o )
{ try {
   database& d = db();

   d.modify(blind_transfer2_settings_id_type(0)(d), [&](blind_transfer2_settings_object& obj) {
      obj.blind_fee = o.blind_fee;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result blind_transfer2_evaluator::do_evaluate( const blind_transfer2_operation& op )
{ try {

   const database& d = db();

   const account_object& from_account = op.from(d);
   to_account_ptr = &op.to(d);

   const asset_object& asset_type = op.amount.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",op.from)
         ("asset",op.amount.asset_id)
      );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, *to_account_ptr, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",op.to)
         ("asset",op.amount.asset_id)
      );
      GRAPHENE_ASSERT(
         not_restricted_account( d, from_account, directionality_type::payer),
         transfer_from_account_restricted,
         "'from' account ${from} is restricted by committee",
         ("from",op.from)
      );
      GRAPHENE_ASSERT(
         not_restricted_account( d, *to_account_ptr, directionality_type::receiver),
         transfer_to_account_restricted,
         "'to' account ${to} is restricted by committee",
         ("to",op.to)
      );

      if (asset_type.is_transfer_restricted())
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account_ptr->id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", op.amount.asset_id)
         );
      }

      const blind_transfer2_settings_object& s = d.get(blind_transfer2_settings_id_type(0));
      FC_ASSERT(op.amount.asset_id == s.blind_fee.asset_id, "assets are different('${a}','${b}')!", ("a",op.amount.asset_id)("b",s.blind_fee.asset_id));

      bool insufficient_balance = (d.get_balance(from_account, asset_type).amount + s.blind_fee.amount) >= op.amount.amount;
      FC_ASSERT(insufficient_balance,
                "Insufficient Balance: ${balance}(fee: ${fee}), unable to make blind transfer '${total_transfer}' from account '${a}' to '${t}'",
                ("a",from_account.name)("t",to_account_ptr->name)
                ("total_transfer",d.to_pretty_string(op.amount))
                ("balance",d.to_pretty_string(d.get_balance(from_account, asset_type)))
                ("fee",d.to_pretty_string(s.blind_fee.amount)) );

      asset_dyn_data_ptr = &asset_type.dynamic_asset_data_id(d);

      // see also 'asset_reserve_operation'
      if (to_account_ptr->burning_mode_enabled)
      {
         FC_ASSERT(!asset_type.is_market_issued(), "Cannot reserve (burn) ${sym} because it is a market-issued asset", ("sym", asset_type.symbol));
         FC_ASSERT( (asset_dyn_data_ptr->current_supply - op.amount.amount) >= 0 );
      }

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

}  FC_CAPTURE_AND_RETHROW( (op) ) }

void_result blind_transfer2_evaluator::do_apply( const blind_transfer2_operation& o )
{ try {
   database& d = db();
   const blind_transfer2_settings_object& s = d.get(blind_transfer2_settings_id_type(0));

   asset am = o.amount + s.blind_fee;
   d.adjust_balance(o.from, -am);

   if (asset_dyn_data_ptr)
   {
      d.modify(*asset_dyn_data_ptr, [&]( asset_dynamic_data_object& data) {
         data.current_supply -= s.blind_fee.amount;
      });
   }

   if (to_account_ptr)
   {
      // normal accrual
      if (!to_account_ptr->burning_mode_enabled) {
         d.adjust_balance(o.to, o.amount);
      }
      // burning
      else if (asset_dyn_data_ptr)
      {
         d.modify(*asset_dyn_data_ptr, [&]( asset_dynamic_data_object& data) {
            data.current_supply -= o.amount.amount;
         });
      }

      // new blind_transfer2 object
      d.create<blind_transfer2_object>([&](blind_transfer2_object& obj)
      {
         obj.from     = o.from;
         obj.to       = o.to;
         obj.amount   = o.amount;
         obj.datetime = d.head_block_time();
         obj.memo     = o.memo;
         obj.fee      = s.blind_fee;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

/////////////////////////////////////////////////////////////////////////////////////////////

void_result override_transfer_evaluator::do_evaluate( const override_transfer_operation& op )
{ try {
   const database& d = db();

   const asset_object&   asset_type      = op.amount.asset_id(d);
   GRAPHENE_ASSERT(
      asset_type.can_override(),
      override_transfer_not_permitted,
      "override_transfer not permitted for asset ${asset}",
      ("asset", op.amount.asset_id)
      );
   FC_ASSERT( asset_type.issuer == op.issuer );

   const account_object& from_account    = op.from(d);
   to_account_ptr = &op.to(d);

   FC_ASSERT( is_authorized_asset( d, *to_account_ptr, asset_type ) );
   FC_ASSERT( is_authorized_asset( d, from_account, asset_type ) );

   FC_ASSERT( not_restricted_account( d, from_account, directionality_type::payer) );
   FC_ASSERT( not_restricted_account( d, *to_account_ptr, directionality_type::receiver) );

   if( d.head_block_time() <= HARDFORK_419_TIME )
   {
      FC_ASSERT( is_authorized_asset( d, from_account, asset_type ) );
   }
   // the above becomes no-op after hardfork because this check will then be performed in evaluator

   FC_ASSERT( d.get_balance( from_account, asset_type ).amount >= op.amount.amount,
              "", ("total_transfer",op.amount)("balance",d.get_balance(from_account, asset_type).amount) );

   if (to_account_ptr->burning_mode_enabled)
   {
      FC_ASSERT(!asset_type.is_market_issued(), "Cannot reserve (burn) ${sym} because it is a market-issued asset", ("sym", asset_type.symbol));

      asset_dyn_data_ptr = &asset_type.dynamic_asset_data_id(d);
      FC_ASSERT( (asset_dyn_data_ptr->current_supply - op.amount.amount) >= 0 );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result override_transfer_evaluator::do_apply( const override_transfer_operation& o )
{ try {
   database& d = db();

   d.adjust_balance( o.from, -o.amount );

   if (to_account_ptr)
   {
      if (!to_account_ptr->burning_mode_enabled) {
         d.adjust_balance( o.to, o.amount );
      }
      else if (asset_dyn_data_ptr)
      {
         d.modify( *asset_dyn_data_ptr, [&]( asset_dynamic_data_object& data ) {
            data.current_supply -= o.amount.amount;
         });
      }
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain
