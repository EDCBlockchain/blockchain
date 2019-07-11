#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/receipt_object.hpp>

namespace graphene { namespace chain {

      class receipt_create_evaluator: public evaluator<receipt_create_evaluator>
      {
      public:
         typedef receipt_create_operation operation_type;

         void_result do_evaluate(const receipt_create_operation& op);
         object_id_type do_apply(const receipt_create_operation& op);

      };

      class receipt_use_evaluator: public evaluator<receipt_use_evaluator>
      {
      public:
         typedef receipt_use_operation operation_type;

         void_result do_evaluate(const receipt_use_operation& op);
         void_result do_apply(const receipt_use_operation& op);

         const receipt_object* receipt_obj_ptr = nullptr;

      };

      class receipt_undo_evaluator: public evaluator<receipt_undo_evaluator>
      {
      public:
         typedef receipt_undo_operation operation_type;

         void_result do_evaluate(const receipt_undo_operation& op);
         void_result do_apply(const receipt_undo_operation& op);

         const receipt_object* receipt_obj_ptr = nullptr;
      };

      /////////////////////////////////////////////////////
} } // graphene::chain