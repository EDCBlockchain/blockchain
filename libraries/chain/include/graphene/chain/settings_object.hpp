#pragma once
#include <graphene/chain/protocol/operations.hpp>
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
      // maximum EDC per account/day
      share_type edc_transfers_daily_limit = 3000000000;
      // fee for creating of market address
      share_type create_market_address_fee_edc = 1;

      settings_id_type get_id() { return id; }
   };

}}

FC_REFLECT_DERIVED( graphene::chain::settings_object,
                    (graphene::db::object),
                    (transfer_fees)
                    (blind_transfer_fees)
                    (cheque_fees)
                    (blind_transfer_default_fee)
                    (edc_deposit_max_sum)
                    (edc_transfers_daily_limit)
                    (create_market_address_fee_edc) )








