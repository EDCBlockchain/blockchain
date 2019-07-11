#include <fc/smart_ref_impl.hpp>
#include <graphene/chain/receipt_evaluator.hpp>
#include <graphene/chain/receipt_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <iostream>

namespace graphene { namespace chain {

void_result receipt_create_evaluator::do_evaluate( const receipt_create_operation& op )
{ try {

   database& d = db();

   FC_ASSERT(d.find_object(op.maker), "Account ${a} doesn't exist", ("a", op.maker));
   FC_ASSERT(d.find_object(op.amount.asset_id), "Asset ${a} doesn't exist", ("a", op.amount.asset_id));

   FC_ASSERT(op.expiration_datetime > d.head_block_time()
             , "Invalid 'expiration_datetime': ${a}. Head block time: ${b}"
             , ("a", op.expiration_datetime)("b", d.head_block_time()));

   const account_object& from_acc = op.maker(d);
   const asset_object& asset_obj  = op.amount.asset_id(d);

   bool insufficient_balance = (d.get_balance(from_acc, asset_obj).amount >= op.amount.amount);
   FC_ASSERT( insufficient_balance,
              "Insufficient balance: ${balance}, unable to create receipt",
              ("balance", d.to_pretty_string(d.get_balance(from_acc, asset_obj))) );

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type receipt_create_evaluator::do_apply( const receipt_create_operation& op )
{ try {

   database& d = db();

   d.adjust_balance(op.maker, -op.amount);

   auto next_receipt_id = d.get_index_type<receipt_index>().get_next_id();

   const receipt_object& new_receipt =
   d.create<receipt_object>([&](receipt_object& o)
                         {
                            o.maker  = op.maker;
                            o.amount = op.amount;
                            o.datetime_creation   = d.head_block_time();
                            o.datetime_expiration = op.expiration_datetime;
                            o.receipt_code   = op.receipt_code;
                            o.receipt_status = receipt_new;
                         });

   FC_ASSERT(new_receipt.id == next_receipt_id);

   return next_receipt_id;

} FC_CAPTURE_AND_RETHROW( (op) ) }

///////////////////////////////////////////////////////////////////////////

void_result receipt_use_evaluator::do_evaluate( const receipt_use_operation& op )
{ try {

   database& d = db();

   FC_ASSERT(d.find_object(op.taker), "Account ${a} doesn't exist", ("a", op.taker));

   const auto& idx = d.get_index_type<receipt_index>().indices().get<by_code>();
   auto itr = idx.find(op.receipt_code);
   FC_ASSERT( itr != idx.end(), "Where is no receipt with code '${id}'!", ("id", op.receipt_code) );

   receipt_obj_ptr = &(*itr);
   FC_ASSERT((receipt_obj_ptr->get_receipt_status() == receipt_new), "Receipt code '${receipt_code}' already used", ("receipt_code", op.receipt_code));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result receipt_use_evaluator::do_apply( const receipt_use_operation& op )
{ try {

   FC_ASSERT(receipt_obj_ptr);

   database& d = db();

   const receipt_object& receipt = *receipt_obj_ptr;

   // adjust balance from receipt to target account
   d.adjust_balance(op.taker, receipt.amount);

   d.modify(receipt, [&](chain::receipt_object& o) {
      o.taker = op.taker;
      o.datetime_used = d.head_block_time();
      o.receipt_status = receipt_used;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

///////////////////////////////////////////////////////////////////////////

void_result receipt_undo_evaluator::do_evaluate( const receipt_undo_operation& op )
{ try {

   database& d = db();
   const auto& idx = d.get_index_type<receipt_index>().indices().get<by_code>();
   auto itr = idx.find(op.receipt_code);
   FC_ASSERT( itr != idx.end(), "Where is no receipt with code '${id}'!", ("id", op.receipt_code) );

   receipt_obj_ptr = &(*itr);
   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result receipt_undo_evaluator::do_apply( const receipt_undo_operation& op )
{ try {

   FC_ASSERT(receipt_obj_ptr);

   database& d = db();

   const receipt_object& receipt = *receipt_obj_ptr;

   // return amount to the owner balance
   d.adjust_balance(receipt.get_maker(), receipt.amount);

   d.modify(receipt, [&](chain::receipt_object& o)
   {
      // when undo receipt, then taker is a maker. Because money back to the maker
      o.taker          = receipt.maker;
      o.datetime_used  = d.head_block_time();
      o.receipt_status = receipt_undo;
   });

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain