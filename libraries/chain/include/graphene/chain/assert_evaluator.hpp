// see LICENSE.txt

#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class assert_evaluator : public evaluator<assert_evaluator>
   {
      public:
         typedef assert_operation operation_type;

         void_result do_evaluate( const assert_operation& o );
         void_result do_apply( const assert_operation& o );
   };

} } // graphene::chain
