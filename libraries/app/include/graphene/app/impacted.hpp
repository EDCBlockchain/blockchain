// see LICENSE.txt

#pragma once

#include <fc/container/flat.hpp>
#include <graphene/protocol/operations.hpp>
#include <graphene/protocol/transaction.hpp>
#include <graphene/protocol/types.hpp>

namespace graphene {

namespace chain { class database; }

namespace app {

void operation_get_impacted_items(
   const graphene::chain::operation& op
   , fc::flat_set<graphene::chain::account_id_type>& result_acc
   , fc::flat_set<chain::fund_id_type>& result_fund
   , graphene::chain::database* db_ptr = nullptr);

void transaction_get_impacted_items(
   const graphene::chain::transaction& tx
   , fc::flat_set<graphene::chain::account_id_type>& result_acc
   , fc::flat_set<chain::fund_id_type>& result_fund
   , graphene::chain::database* db_ptr = nullptr);

//////////////////////////////////////////////////
//
//void operation_get_impacted_funds(
//   const graphene::chain::operation& op,
//   fc::flat_set<chain::fund_id_type>& result);

} } // graphene::app
