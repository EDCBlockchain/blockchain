#pragma once

#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

   inline void check_fund_options(const fund_options& opts, const database& db, fund_payment_scheme p_scheme)
   { try {
      // maximum payment to users (per day) must be less than minimum payment to fund
      auto iter_payment_rate_max = std::max_element(opts.payment_rates.begin(), opts.payment_rates.end(),
         [](const fund_options::payment_rate& item1, const fund_options::payment_rate& item2) {
            return (item1.percent < item2.percent);
         });
      auto iter_fund_rate_min = std::min_element(opts.fund_rates.begin(), opts.fund_rates.end(),
         [&db](const fund_options::fund_rate& item1, const fund_options::fund_rate& item2)
         {
            if (db.head_block_time() >= HARDFORK_626_TIME) {
               return (item1.day_percent < item2.day_percent);
            }
            return (item1.day_percent > item2.day_percent);
         });
      FC_ASSERT( ((iter_payment_rate_max != opts.payment_rates.end())
                  && (iter_fund_rate_min != opts.fund_rates.end())), "wrong 'options.fund_rates/options.payment_rates' parameters [0]!" );

      if ( (db.head_block_time() < HARDFORK_632_TIME) || (p_scheme != fund_payment_scheme::fixed) )
      {
         bool percent_valid = (iter_payment_rate_max->percent / iter_payment_rate_max->period) <= iter_fund_rate_min->day_percent;
         FC_ASSERT(percent_valid, "wrong 'options.fund_rates/options.payment_rates' parameters [1]!");
      }

      // minimum payment to fund must be greater or equal than reduction for whole period
      bool rates_reduction_valid = true;
      if (db.head_block_time() >= HARDFORK_626_TIME) {
         rates_reduction_valid = (uint32_t)(opts.rates_reduction_per_month * opts.period / 30) <= iter_fund_rate_min->day_percent;
      }
      else {
         rates_reduction_valid = (uint32_t)(opts.rates_reduction_per_month / 30 * opts.period) <= iter_fund_rate_min->day_percent;
      }
      FC_ASSERT(rates_reduction_valid, "invalid settings.rates_reduction_per_month (${a})!", ("a", opts.rates_reduction_per_month));

   } FC_CAPTURE_AND_RETHROW( (opts) ) }

   class fund_create_evaluator: public evaluator<fund_create_evaluator>
   {
   public:
      typedef fund_create_operation operation_type;

      void_result do_evaluate(const fund_create_operation& o);
      object_id_type do_apply(const fund_create_operation& o);

   };

   /////////////////////////////////////////////////////

   class fund_update_evaluator: public evaluator<fund_update_evaluator>
   {
   public:
      typedef fund_update_operation operation_type;

      void_result do_evaluate(const fund_update_operation& o);
      void_result do_apply(const fund_update_operation& o);

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_refill_evaluator: public evaluator<fund_refill_evaluator>
   {
   public:
      typedef fund_refill_operation operation_type;

      void_result do_evaluate(const fund_refill_operation& o);
      void_result do_apply(const fund_refill_operation& o);

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_evaluator: public evaluator<fund_deposit_evaluator>
   {
   public:
      typedef fund_deposit_operation operation_type;

      void_result do_evaluate(const fund_deposit_operation& o);
      operation_result do_apply(const fund_deposit_operation& o);

      optional<fund_options::payment_rate> p_rate;
      const fund_object* fund_obj_ptr = nullptr;
      const account_object* account_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_withdrawal_evaluator: public evaluator<fund_withdrawal_evaluator>
   {
   public:
      typedef fund_withdrawal_operation operation_type;

      void_result do_evaluate(const fund_withdrawal_operation& o);
      void_result do_apply(const fund_withdrawal_operation& o);

      const fund_object* fund_obj_ptr = nullptr;
      const account_object* account_ptr = nullptr;
   };

   /////////////////////////////////////////////////////

   class fund_payment_evaluator: public evaluator<fund_payment_evaluator>
   {
   public:
      typedef fund_payment_operation operation_type;

      void_result do_evaluate(const fund_payment_operation& o);
      void_result do_apply(const fund_payment_operation& o);

      const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;
      const account_object* account_ptr = nullptr;
   };

   /////////////////////////////////////////////////////

   class fund_set_enable_evaluator: public evaluator<fund_set_enable_evaluator>
   {
   public:
      typedef fund_set_enable_operation operation_type;

      void_result do_evaluate(const fund_set_enable_operation& o);
      void_result do_apply(const fund_set_enable_operation& o);

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_set_enable_evaluator: public evaluator<fund_deposit_set_enable_evaluator>
   {
   public:
      typedef fund_deposit_set_enable_operation operation_type;

      void_result do_evaluate(const fund_deposit_set_enable_operation& o);
      void_result do_apply(const fund_deposit_set_enable_operation& o);

      const fund_deposit_object* fund_dep_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_remove_evaluator: public evaluator<fund_remove_evaluator>
   {
   public:
      typedef fund_remove_operation operation_type;

      void_result do_evaluate(const fund_remove_operation& o);
      void_result do_apply(const fund_remove_operation& o);

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_change_payment_scheme_evaluator: public evaluator<fund_change_payment_scheme_evaluator>
   {
   public:
      typedef fund_change_payment_scheme_operation operation_type;

      void_result do_evaluate(const fund_change_payment_scheme_operation& o);
      void_result do_apply(const fund_change_payment_scheme_operation& o);

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class enable_autorenewal_deposits_evaluator: public evaluator<enable_autorenewal_deposits_evaluator>
   {
   public:
      typedef enable_autorenewal_deposits_operation operation_type;

      void_result do_evaluate(const enable_autorenewal_deposits_operation& o);
      void_result do_apply(const enable_autorenewal_deposits_operation& o);

      const account_object* account_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class deposit_renewal_evaluator: public evaluator<deposit_renewal_evaluator>
   {
   public:
      typedef deposit_renewal_operation operation_type;

      void_result do_evaluate( const deposit_renewal_operation& o );
      operation_result do_apply( const deposit_renewal_operation& o );

      const fund_deposit_object* fund_deposit_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_update_evaluator: public evaluator<fund_deposit_update_evaluator>
   {
   public:
      typedef fund_deposit_update_operation operation_type;

      void_result do_evaluate( const fund_deposit_update_operation& o );
      dep_update_info do_apply( const fund_deposit_update_operation& o );

      const fund_object* fund_obj_ptr = nullptr;
      optional<fund_options::payment_rate> p_rate;
      const fund_deposit_object* fund_deposit_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_update2_evaluator: public evaluator<fund_deposit_update2_evaluator>
   {
   public:
      typedef fund_deposit_update2_operation operation_type;

      void_result do_evaluate( const fund_deposit_update2_operation& o );
      dep_update_info do_apply( const fund_deposit_update2_operation& o );

      const fund_object* fund_obj_ptr = nullptr;
      optional<fund_options::payment_rate> p_rate;
      const fund_deposit_object* fund_deposit_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_reduce_evaluator: public evaluator<fund_deposit_reduce_evaluator>
   {
   public:
      typedef fund_deposit_reduce_operation operation_type;

      void_result do_evaluate( const fund_deposit_reduce_operation& o );
      asset do_apply( const fund_deposit_reduce_operation& o );

      const fund_object* fund_obj_ptr = nullptr;
      const fund_deposit_object* fund_deposit_ptr = nullptr;
      const asset_dynamic_data_object* asset_dyn_data_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class fund_deposit_acceptance_evaluator: public evaluator<fund_deposit_acceptance_evaluator>
   {
   public:
      typedef fund_deposit_acceptance_operation operation_type;

      void_result do_evaluate( const fund_deposit_acceptance_operation& o );
      void_result do_apply( const fund_deposit_acceptance_operation& o );

      const fund_object* fund_obj_ptr = nullptr;

   };

   /////////////////////////////////////////////////////

   class hard_enable_autorenewal_deposits_evaluator: public evaluator<hard_enable_autorenewal_deposits_evaluator>
   {
   public:
      typedef hard_enable_autorenewal_deposits_operation operation_type;

      void_result do_evaluate(const hard_enable_autorenewal_deposits_operation& o);
      void_result do_apply(const hard_enable_autorenewal_deposits_operation& o);

   };


} } // graphene::chain
