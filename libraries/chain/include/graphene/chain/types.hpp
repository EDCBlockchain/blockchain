/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
