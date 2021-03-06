// see LICENSE.txt

#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/types.hpp>

namespace graphene { namespace chain {

   class account_object;
   class asset_object;
   class asset_bitasset_data_object;
   class call_order_object;
   class limit_order_object;

   class limit_order_create_evaluator : public evaluator<limit_order_create_evaluator>
   {
      public:
         typedef limit_order_create_operation operation_type;

         void_result do_evaluate( const limit_order_create_operation& o );
         object_id_type do_apply( const limit_order_create_operation& o );

         asset calculate_market_fee( const asset_object* aobj, const asset& trade_amount );

         /** override the default behavior defined by generic_evalautor which is to
          * post the fee to fee_paying_account_stats.pending_fees
          */
         virtual void pay_fee() override;

         share_type                          _deferred_fee  = 0;
         const limit_order_create_operation* _op            = nullptr;
         const account_object*               _seller        = nullptr;
         const asset_object*                 _sell_asset    = nullptr;
         const asset_object*                 _receive_asset = nullptr;
   };

   class limit_order_cancel_evaluator : public evaluator<limit_order_cancel_evaluator>
   {
      public:
         typedef limit_order_cancel_operation operation_type;

         void_result do_evaluate( const limit_order_cancel_operation& o );
         asset do_apply( const limit_order_cancel_operation& o );

         const limit_order_object* _order;
   };

   class call_order_update_evaluator : public evaluator<call_order_update_evaluator>
   {
      public:
         typedef call_order_update_operation operation_type;

         void_result do_evaluate( const call_order_update_operation& o );
         void_result do_apply( const call_order_update_operation& o );

         bool _closing_order = false;
         const asset_object* _debt_asset = nullptr;
         const account_object* _paying_account = nullptr;
         const call_order_object* _order = nullptr;
         const asset_bitasset_data_object* _bitasset_data = nullptr;
   };

} } // graphene::chain
