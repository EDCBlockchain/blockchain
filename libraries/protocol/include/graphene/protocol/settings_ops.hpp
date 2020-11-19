#pragma once

#include <graphene/protocol/referral_classes.hpp>
#include <graphene/protocol/ext.hpp>

namespace graphene { namespace protocol {

   struct settings_fee
   {
      asset_id_type asset_id;
      int64_t percent = 0;
   };

   struct settings_ranks
   {
      share_type rank1_edc_amount = 0;
      share_type rank2_edc_amount = 0;
      share_type rank3_edc_amount = 0;
      int64_t rank1_edc_transfer_fee_percent = 0;
      int64_t rank2_edc_transfer_fee_percent = 0;
      int64_t rank3_edc_transfer_fee_percent = 0;
   };

   /**
    * @ingroup operations
    */
   struct update_settings_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      struct ext
      {
         optional<void_t> null_ext;
         optional<std::vector<settings_fee>> cheque_fees;
         optional<share_type> create_market_address_fee_edc;
         optional<share_type> edc_cheques_daily_limit;
         optional<asset> block_reward;
         optional<uint32_t> witness_fees_percent;
         optional<settings_ranks> ranks;
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

   };

   /**
    * @ingroup operations
    */
   struct update_referral_settings_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      bool referral_payments_enabled = false;
      uint32_t referral_level1_percent = 0;
      uint32_t referral_level2_percent = 0;
      uint32_t referral_level3_percent = 0;
      std::pair<share_type, share_type> referral_min_limit_edc_level1;
      std::pair<share_type, share_type> referral_min_limit_edc_level2;
      std::pair<share_type, share_type> referral_min_limit_edc_level3;

      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::settings_fee, (asset_id)(percent) )
FC_REFLECT( graphene::protocol::settings_ranks,
            (rank1_edc_amount)
            (rank2_edc_amount)
            (rank3_edc_amount)
            (rank1_edc_transfer_fee_percent)
            (rank2_edc_transfer_fee_percent)
            (rank3_edc_transfer_fee_percent) )
FC_REFLECT( graphene::protocol::update_settings_operation::ext,
            (null_ext)
            (cheque_fees)
            (create_market_address_fee_edc)
            (edc_cheques_daily_limit)
            (block_reward)
            (witness_fees_percent)
            (ranks) )

FC_REFLECT( graphene::protocol::update_settings_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::update_referral_settings_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::update_settings_operation,
            (fee)
            (transfer_fees)
            (blind_transfer_fees)
            (blind_transfer_default_fee)
            (edc_deposit_max_sum)
            (edc_transfers_daily_limit)(extensions)
          )
FC_REFLECT( graphene::protocol::update_referral_settings_operation,
            (fee)
            (referral_payments_enabled)
            (referral_level1_percent)
            (referral_level2_percent)
            (referral_level3_percent)
            (referral_min_limit_edc_level1)
            (referral_min_limit_edc_level2)
            (referral_min_limit_edc_level3)
          )

// ! implementations in small_ops.cpp

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_settings_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_referral_settings_operation::fee_parameters_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_settings_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_referral_settings_operation )