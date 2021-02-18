// see LICENSE.txt

#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/committee_member_object.hpp>

namespace graphene { namespace chain {

   class committee_member_create_evaluator : public evaluator<committee_member_create_evaluator>
   {
      public:
         typedef committee_member_create_operation operation_type;

         void_result do_evaluate( const committee_member_create_operation& o );
         object_id_type do_apply( const committee_member_create_operation& o );
   };

   class committee_member_update_evaluator : public evaluator<committee_member_update_evaluator>
   {
      public:
         typedef committee_member_update_operation operation_type;

         void_result do_evaluate( const committee_member_update_operation& o );
         void_result do_apply( const committee_member_update_operation& o );
   };

   class committee_member_update_global_parameters_evaluator : public evaluator<committee_member_update_global_parameters_evaluator>
   {
      public:
         typedef committee_member_update_global_parameters_operation operation_type;

         void_result do_evaluate( const committee_member_update_global_parameters_operation& o );
         void_result do_apply( const committee_member_update_global_parameters_operation& o );
   };

} } // graphene::chain
