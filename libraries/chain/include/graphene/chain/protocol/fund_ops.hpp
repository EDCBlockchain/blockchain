#pragma once

#include <graphene/chain/protocol/referral_classes.hpp>
#include <graphene/chain/protocol/ext.hpp>

namespace graphene { namespace chain {

   struct fund_options
   {
      struct payment_rate
      {
         // deposit period
         uint32_t period = 0;
         // percent for whole period
         uint32_t percent = 0;
      };

      struct fund_rate
      {
         share_type amount = 0;
         uint32_t day_percent = 0;
      };

      void validate() const;

      std::string description;
      uint16_t    period = 0;
      share_type  min_deposit = 0;
      uint16_t    rates_reduction_per_month = 0;

      std::vector<fund_rate>    fund_rates;
      std::vector<payment_rate> payment_rates;

   }; // fund_options

   /**
    * @ingroup operations
    */
   struct fund_create_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      std::string     name;
      account_id_type owner;
      asset_id_type   asset_id;
      fund_options    options;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_create_operation

   /**
    * @ingroup operations
    */
   struct fund_update_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    id; // fund id
      fund_options    options;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_update_operation

   /**
    * @ingroup operations
    */
   struct fund_refill_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    id; // fund id
      share_type      amount;

      extensions_type extensions;
      account_id_type fee_payer() const { return from_account; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_refill_operation

   /**
    * @ingroup operations
    */
   struct fund_deposit_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      account_id_type from_account;
      fund_id_type    id; // fund id
      share_type      amount;
      uint32_t        period;

      extensions_type extensions;
      account_id_type fee_payer() const { return from_account; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_deposit_create_operation

   /**
    * @ingroup operations
    */
   struct fund_withdrawal_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type issuer;

      fund_id_type       fund_id;
      asset              asset_to_issue; // amount
      account_id_type    issue_to_account;
      fc::time_point_sec datetime; // block datetime

      extensions_type extensions;
      account_id_type fee_payer() const { return issuer; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_deposit_create_operation

   /**
    * @ingroup operations
    */
   struct fund_payment_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type issuer;

      fund_id_type    fund_id;
      optional<fund_deposit_id_type> deposit_id;

      asset           asset_to_issue; // amount to user
      account_id_type issue_to_account;

      extensions_type extensions;
      account_id_type fee_payer() const { return issuer; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_payment_operation

   /**
    * @ingroup operations
    */
   struct fund_set_enable_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      fund_id_type    id; // fund id
      bool            enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_set_enable_operation

   struct fund_deposit_set_enable_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_deposit_id_type id; // fund deposit id
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_deposit_set_enable_operation

   struct fund_remove_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_id_type id; // fund id

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_remove_operation

   struct fund_set_fixed_percent_on_deposits_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fund_id_type id; // fund id
      uint32_t percent = 0;

      extensions_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // fund_set_fixed_percent_on_deposits

   struct enable_autorenewal_deposits_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      account_id_type account;
      bool enabled = true;

      extensions_type extensions;
      account_id_type fee_payer() const { return account; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // enable_autorenewal_deposits_operation

} } // graphene::chain

FC_REFLECT( graphene::chain::fund_options::payment_rate,
            (period)(percent) )

FC_REFLECT( graphene::chain::fund_options::fund_rate,
            (amount)(day_percent) )

FC_REFLECT( graphene::chain::fund_options,
            (description)(period)(min_deposit)(rates_reduction_per_month)(fund_rates)(payment_rates) )

FC_REFLECT( graphene::chain::fund_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_create_operation,
            (fee)(name)(owner)(asset_id)(options)(extensions) )

FC_REFLECT( graphene::chain::fund_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_update_operation,
            (fee)(from_account)(id)(options)(extensions) )

FC_REFLECT( graphene::chain::fund_refill_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_refill_operation,
            (fee)(from_account)(id)(amount)(extensions) )

FC_REFLECT( graphene::chain::fund_deposit_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_deposit_operation,
            (fee)(from_account)(id)(amount)(period)(extensions) )

FC_REFLECT( graphene::chain::fund_withdrawal_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_withdrawal_operation,
            (fee)(issuer)(fund_id)(asset_to_issue)(issue_to_account)(datetime)(extensions) )

FC_REFLECT( graphene::chain::fund_payment_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_payment_operation,
            (fee)(issuer)(fund_id)(deposit_id)(asset_to_issue)(issue_to_account)(extensions) )

FC_REFLECT( graphene::chain::fund_set_enable_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_set_enable_operation,
            (fee)(id)(enabled)(extensions) )

FC_REFLECT( graphene::chain::fund_deposit_set_enable_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_deposit_set_enable_operation,
            (fee)(id)(enabled)(extensions) )

FC_REFLECT( graphene::chain::fund_remove_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_remove_operation,
            (fee)(id)(extensions) )

FC_REFLECT( graphene::chain::fund_set_fixed_percent_on_deposits_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::fund_set_fixed_percent_on_deposits_operation,
            (fee)(id)(percent)(extensions) )

FC_REFLECT( graphene::chain::enable_autorenewal_deposits_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::enable_autorenewal_deposits_operation,
            (fee)(account)(enabled)(extensions) )

