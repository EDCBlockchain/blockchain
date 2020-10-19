#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class witnesses_info_object;

   class witnesses_info_evaluator: public evaluator<witnesses_info_evaluator>
   {
   public:
      typedef set_witness_exception_operation operation_type;

      void_result do_evaluate( const set_witness_exception_operation& o );
      void_result do_apply( const set_witness_exception_operation& o );

      const witnesses_info_object* info_ptr = nullptr;

   };

} } // graphene::chain
