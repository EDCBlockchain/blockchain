#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>
#include <graphene/chain/database.hpp>

#include <iostream>

namespace graphene { namespace chain {

   /**
    * @class fund_object
    * @ingroup object
    * @ingroup protocol
    *
    * This object contains custom settings of blockchain
    */
   class settings_object : public abstract_object<settings_object>
   {
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id  = impl_settings_object_type;

      // fee percents for transfers
      std::vector<chain::settings_fee> transfer_fees;
      // fee percents for blind transfers
      std::vector<chain::settings_fee> blind_transfer_fees;
      // fee percents for cheques
      std::vector<chain::settings_fee> cheque_fees;
      // fixed fee for blind transfer
      asset blind_transfer_default_fee;
      // max sum of deposits for each user for EDC
      share_type edc_deposit_max_sum = 30000000000;
      // maximum EDC per account/day for transfers
      share_type edc_transfers_daily_limit = 3000000000;
      // maximum EDC per account/day for blind cheques
      share_type edc_cheques_daily_limit = 3000000000;
      // fee for creating of market address
      share_type create_market_address_fee_edc = 1;
      // reward for block signing
      asset block_reward;
      // for paying to current witnesses some percent of all fees between two MT
      uint16_t witness_fees_percent = 0;
      // make denominate during the next MT
      bool make_denominate = false;
      asset_id_type denominate_asset;
      uint32_t denominate_coef = 1;

      // ranks
      share_type rank1_edc_amount = 1000000000;
      share_type rank2_edc_amount = 5000000000;
      share_type rank3_edc_amount = 10000000000;
      int64_t rank1_edc_transfer_fee_percent = 3000;
      int64_t rank2_edc_transfer_fee_percent = 2000;
      int64_t rank3_edc_transfer_fee_percent = 0;

      settings_id_type get_id() { return id; }
   };

}}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::settings_object)

FC_REFLECT_TYPENAME( graphene::chain::settings_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::settings_object )
