// see LICENSE.txt

#pragma once

#include <fc/exception/exception.hpp>
#include <graphene/protocol/exceptions.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/types.hpp>

#define GRAPHENE_ASSERT( expr, exc_type, FORMAT, ... )                \
   FC_MULTILINE_MACRO_BEGIN                                           \
   if( !(expr) )                                                      \
      FC_THROW_EXCEPTION( exc_type, FORMAT, __VA_ARGS__ );            \
   FC_MULTILINE_MACRO_END

#define GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( op_name )                \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _validate_exception,                                 \
      graphene::chain::operation_validate_exception,                  \
      3040000 + 100 * operation::tag< op_name ## _operation >::value  \
      )                                                               \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _evaluate_exception,                                 \
      graphene::chain::operation_evaluate_exception,                  \
      3050000 + 100 * operation::tag< op_name ## _operation >::value  \
      )

#define GRAPHENE_IMPLEMENT_OP_BASE_EXCEPTIONS( op_name )              \
   FC_IMPLEMENT_DERIVED_EXCEPTION(                                    \
      op_name ## _validate_exception,                                 \
      graphene::chain::operation_validate_exception,                  \
      3040000 + 100 * operation::tag< op_name ## _operation >::value, \
      #op_name "_operation validation exception"                      \
      )                                                               \
   FC_IMPLEMENT_DERIVED_EXCEPTION(                                    \
      op_name ## _evaluate_exception,                                 \
      graphene::chain::operation_evaluate_exception,                  \
      3050000 + 100 * operation::tag< op_name ## _operation >::value, \
      #op_name "_operation evaluation exception"                      \
      )

#define GRAPHENE_DECLARE_OP_VALIDATE_EXCEPTION( exc_name, op_name, seqnum ) \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _validate_exception,                \
      3040000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum                                                     \
      )

#define GRAPHENE_IMPLEMENT_OP_VALIDATE_EXCEPTION( exc_name, op_name, seqnum, msg ) \
   FC_IMPLEMENT_DERIVED_EXCEPTION(                                    \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _validate_exception,                \
      3040000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum,                                                    \
      msg                                                             \
      )

#define GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( exc_name, op_name, seqnum ) \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _evaluate_exception,                \
      3050000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum                                                     \
      )

#define GRAPHENE_IMPLEMENT_OP_EVALUATE_EXCEPTION( exc_name, op_name, seqnum, msg ) \
   FC_IMPLEMENT_DERIVED_EXCEPTION(                                    \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _evaluate_exception,                \
      3050000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum,                                                    \
      msg                                                             \
      )

#define GRAPHENE_TRY_NOTIFY( signal, ... )                                    \
   try                                                                        \
   {                                                                          \
      signal( __VA_ARGS__ );                                                  \
   }                                                                          \
   catch( const graphene::chain::plugin_exception& e )                        \
   {                                                                          \
      elog( "Caught plugin exception: ${e}", ("e", e.to_detail_string() ) );  \
      throw;                                                                  \
   }                                                                          \
   catch( ... )                                                               \
   {                                                                          \
      wlog( "Caught unexpected exception in plugin" );                        \
   }

namespace graphene { namespace chain {

      FC_DECLARE_EXCEPTION( chain_exception, 3000000);
      FC_DECLARE_DERIVED_EXCEPTION( database_query_exception,          graphene::chain::chain_exception, 3010000)
      FC_DECLARE_DERIVED_EXCEPTION( block_validate_exception,          graphene::chain::chain_exception, 3020000)
      FC_DECLARE_DERIVED_EXCEPTION( transaction_process_exception,     graphene::chain::chain_exception, 3030000)
      FC_DECLARE_DERIVED_EXCEPTION( transaction_exception,             graphene::chain::chain_exception, 3040000)
      FC_DECLARE_DERIVED_EXCEPTION( operation_validate_exception,      graphene::chain::chain_exception, 3050000)
      FC_DECLARE_DERIVED_EXCEPTION( operation_evaluate_exception,      graphene::chain::chain_exception, 3060000)
      FC_DECLARE_DERIVED_EXCEPTION( utility_exception,                 graphene::chain::chain_exception, 3070000)
      FC_DECLARE_DERIVED_EXCEPTION( undo_database_exception,           graphene::chain::chain_exception, 3080000)
      FC_DECLARE_DERIVED_EXCEPTION( unlinkable_block_exception,        graphene::chain::chain_exception, 3090000)
      FC_DECLARE_DERIVED_EXCEPTION( black_swan_exception,              graphene::chain::chain_exception, 3100000)

      FC_DECLARE_DERIVED_EXCEPTION( tx_missing_owner_auth,             graphene::chain::transaction_exception, 3030002)
      FC_DECLARE_DERIVED_EXCEPTION( duplicate_transaction,             transaction_process_exception,          3030008 )

      FC_DECLARE_DERIVED_EXCEPTION( invalid_pts_address,               graphene::chain::utility_exception, 3060001)
      FC_DECLARE_DERIVED_EXCEPTION( insufficient_feeds,                graphene::chain::chain_exception, 37006)

      FC_DECLARE_DERIVED_EXCEPTION( pop_empty_chain,                   graphene::chain::undo_database_exception, 3070001)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( transfer );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( from_account_not_whitelisted, transfer, 1)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( to_account_not_whitelisted, transfer, 2)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( from_account_restricted, transfer, 3)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( to_account_restricted, transfer, 4)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( restricted_transfer_asset, transfer, 5)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( limit_order_create );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( kill_unfilled, limit_order_create, 1 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( market_not_whitelisted, limit_order_create, 2 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( market_blacklisted, limit_order_create, 3 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( selling_asset_unauthorized, limit_order_create, 4 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( receiving_asset_unauthorized, limit_order_create, 5 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( insufficient_balance, limit_order_create, 6 )

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( limit_order_cancel );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( nonexist_order, limit_order_cancel, 1 )
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( owner_mismatch, limit_order_cancel, 2 )

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( call_order_update );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( unfilled_margin_call, call_order_update, 1)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_create );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( max_auth_exceeded, account_create, 1)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( auth_account_not_found, account_create, 2)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_incorrect_issuer, account_create, 3)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_already_exists, account_create, 4)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_too_many_markets, account_create, 5)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_update );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( max_auth_exceeded, account_update, 1)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( auth_account_not_found, account_update, 2)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_reserve );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( invalid_on_mia, asset_reserve, 1)


      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( proposal_create );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( review_period_required, proposal_create, 1)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( review_period_insufficient, proposal_create, 2)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( balance_claim );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( claimed_too_often, balance_claim, 1)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( invalid_claim_amount, balance_claim, 2)
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( owner_mismatch, balance_claim, 3)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( override_transfer );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( not_permitted, override_transfer, 1)

      GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( blind_transfer );
      GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( unknown_commitment, blind_transfer, 1);


#define GRAPHENE_RECODE_EXC( cause_type, effect_type ) \
      catch( const cause_type& e ) \
      { throw( effect_type( e.what(), e.get_log() ) ); }

} } // graphene::chain

