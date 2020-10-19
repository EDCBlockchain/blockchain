#include <graphene/chain/witnesses_info_evaluator.hpp>
#include <graphene/chain/witnesses_info_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <iostream>

namespace graphene { namespace chain {

void_result witnesses_info_evaluator::do_evaluate( const set_witness_exception_operation& op )
{ try {

   database& d = db();

   auto itr = d.find(witnesses_info_id_type(0));
   FC_ASSERT(itr, "can't find 'witnesses_info_object'!");

   info_ptr = &(*itr);

   FC_ASSERT( (op.exc_accounts_blocks.size() > 0) || (op.exc_accounts_fees.size() > 0)
              , "size of exc_accounts_blocks (or exc_accounts_fees) must be > 0");

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result witnesses_info_evaluator::do_apply( const set_witness_exception_operation& op )
{ try {

   database& d = db();

   d.modify(*info_ptr, [&](witnesses_info_object& obj)
   {
      if (op.exception_enabled)
      {
         for (const account_id_type& acc_id: op.exc_accounts_blocks)
         {
            if (obj.exc_accounts_blocks.find(acc_id) == obj.exc_accounts_blocks.end()) {
               obj.exc_accounts_blocks.insert(acc_id);
            }
         }
         for (const account_id_type& acc_id: op.exc_accounts_fees)
         {
            if (obj.exc_accounts_fees.find(acc_id) == obj.exc_accounts_fees.end()) {
               obj.exc_accounts_fees.insert(acc_id);
            }
         }
      }
      else
      {
         for (const account_id_type& acc_id: op.exc_accounts_blocks)
         {
            auto itr = obj.exc_accounts_blocks.find(acc_id);
            if (itr != obj.exc_accounts_blocks.end()) {
               obj.exc_accounts_blocks.erase(itr);
            }
         }
         for (const account_id_type& acc_id: op.exc_accounts_fees)
         {
            auto itr = obj.exc_accounts_fees.find(acc_id);
            if (itr != obj.exc_accounts_fees.end()) {
               obj.exc_accounts_fees.erase(itr);
            }
         }
      }
   });

   return void_result{};

} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
