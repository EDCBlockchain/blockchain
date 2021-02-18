// see LICENSE.txt

#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class custom_evaluator : public evaluator<custom_evaluator>
   {
      public:
         typedef custom_operation operation_type;

         void_result do_evaluate( const custom_operation& o ){ return void_result(); }
         void_result do_apply( const custom_operation& o ){ return void_result(); }
   };
} }
