// see LICENSE.txt

#pragma once

#include <graphene/protocol/types.hpp>

namespace graphene { namespace chain { using namespace protocol; } }

GRAPHENE_DEFINE_IDS(chain, implementation_ids, impl_,
   (global_property)                      // [idx: 0]
   (dynamic_global_property)
   (reserved0)                            // [idx: 2]
   (asset_dynamic_data)
   (asset_bitasset_data)                  // [idx: 4]  
   (account_balance)
   (account_statistics)                   // [idx: 6]
   (transaction_history)
   (block_summary)                        // [idx: 8]
   (account_transaction_history)
   (blinded_balance)                      // [idx: 10]
   (chain_property)
   (witness_schedule)                     // [idx: 12]
   (budget_record)
   (special_authority)                    // [idx: 14]
   (buyback)
   (fba_accumulator)                      // [idx: 16]
   (account_mature_balance)
   (account_properties)                   // [idx: 18]
   (accounts_online)
   (bonus_balances)                       // [idx: 20]
   (fund_deposit)
   (fund_statistics)                      // [idx: 22]
   (fund_transaction_history)
   (fund_history)                         // [idx: 24]
   (settings)
   (blind_transfer2)                      // [idx: 26]
)
