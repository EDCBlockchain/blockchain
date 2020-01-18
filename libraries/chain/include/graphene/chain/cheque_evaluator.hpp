#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/cheque_object.hpp>

namespace graphene { namespace chain {

   class cheque_create_evaluator: public evaluator<cheque_create_evaluator>
   {
   public:
      typedef cheque_create_operation operation_type;

      void_result do_evaluate(const cheque_create_operation& op);
      object_id_type do_apply(const cheque_create_operation& op);

      const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;
      share_type custom_fee;
      share_type cheque_amount;
   };

   class cheque_use_evaluator: public evaluator<cheque_use_evaluator>
   {
   public:
      typedef cheque_use_operation operation_type;

      void_result do_evaluate(const cheque_use_operation& op);
      object_id_type do_apply(const cheque_use_operation& op);

      const cheque_object* cheque_obj_ptr = nullptr;
   };

   class cheque_reverse_evaluator: public evaluator<cheque_reverse_evaluator>
   {
   public:
      typedef cheque_reverse_operation operation_type;

      void_result do_evaluate(const cheque_reverse_operation& op);
      void_result do_apply(const cheque_reverse_operation& op);

      const cheque_object* cheque_obj_ptr = nullptr;
   };

} } // graphene::chain