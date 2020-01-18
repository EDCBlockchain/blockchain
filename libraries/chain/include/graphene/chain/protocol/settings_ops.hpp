#pragma once

#include <graphene/chain/protocol/referral_classes.hpp>
#include <graphene/chain/protocol/ext.hpp>
#include <regex>

namespace graphene { namespace chain {

   struct settings_fee
   {
      asset_id_type asset_id;
      int64_t percent = 0;
   };

   /**
    * @ingroup operations
    */
   struct update_settings_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      struct ext {
         optional<void_t> null_ext;
         optional<std::vector<settings_fee>> cheque_fees;
         optional<share_type> create_market_address_fee_edc;
      };

      asset fee;

      std::vector<settings_fee> transfer_fees;
      std::vector<settings_fee> blind_transfer_fees;
      asset blind_transfer_default_fee;
      share_type edc_deposit_max_sum;
      share_type edc_transfers_daily_limit;

      extension<ext> extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // update_settings_operation

} } // graphene::chain

FC_REFLECT( graphene::chain::settings_fee,
            (asset_id)(percent) )

FC_REFLECT( graphene::chain::update_settings_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::update_settings_operation::ext, (null_ext)(cheque_fees)(create_market_address_fee_edc) )
FC_REFLECT( graphene::chain::update_settings_operation,
            (fee)(transfer_fees)(blind_transfer_fees)(blind_transfer_default_fee)(edc_deposit_max_sum)(edc_transfers_daily_limit)(extensions))
