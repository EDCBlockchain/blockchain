#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class settings_object;

   class settings_evaluator: public evaluator<settings_evaluator>
   {
   public:
      typedef update_settings_operation operation_type;

      void_result do_evaluate( const update_settings_operation& o );
      void_result do_apply( const update_settings_operation& o );

      const settings_object* settings_ptr = nullptr;

   };

} } // graphene::chain
