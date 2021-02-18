// see LICENSE.txt

#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class settings_object;

   class transfer_evaluator: public evaluator<transfer_evaluator>
   {
      public:
         typedef transfer_operation operation_type;

         void_result do_evaluate( const transfer_operation& o );
         void_result do_apply( const transfer_operation& o );

         const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;
         const account_object* from_account_ptr = nullptr;
         const account_object* to_account_ptr = nullptr;

         const settings_object* settings_ptr = nullptr;

         share_type custom_fee = 0;
   };

   class blind_transfer2_evaluator: public evaluator<blind_transfer2_evaluator>
   {
   public:
      typedef blind_transfer2_operation operation_type;

      void_result do_evaluate( const blind_transfer2_operation& o );
      asset do_apply( const blind_transfer2_operation& o );

      const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;
      const asset_dynamic_data_object* fee_dyn_data_ptr = nullptr;

      const account_object* from_account_ptr = nullptr;
      const account_object* to_account_ptr = nullptr;
      const settings_object* settings_ptr = nullptr;

      asset custom_fee;
   };

   class update_blind_transfer2_settings_evaluator: public evaluator<update_blind_transfer2_settings_evaluator>
   {
   public:
      typedef update_blind_transfer2_settings_operation operation_type;

      void_result do_evaluate( const update_blind_transfer2_settings_operation& o );
      void_result do_apply( const update_blind_transfer2_settings_operation& o );

   };

   class override_transfer_evaluator: public evaluator<override_transfer_evaluator>
   {
      public:
         typedef override_transfer_operation operation_type;

         void_result do_evaluate( const override_transfer_operation& o );
         void_result do_apply( const override_transfer_operation& o );

         const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;
         const account_object*            to_account_ptr = nullptr;
   };

} } // graphene::chain
