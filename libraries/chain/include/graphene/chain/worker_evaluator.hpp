// see LICENSE.txt

#pragma once
#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {

   class worker_create_evaluator : public evaluator<worker_create_evaluator>
   {
      public:
         typedef worker_create_operation operation_type;

         void_result do_evaluate( const operation_type& o );
         object_id_type do_apply( const operation_type& o );
   };

} } // graphene::chain
