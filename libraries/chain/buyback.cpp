// see LICENSE.txt

#include <graphene/protocol/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void evaluate_buyback_account_options( const database& db, const buyback_account_options& bbo )
{
   FC_ASSERT( db.head_block_time() >= HARDFORK_538_TIME );
   const asset_object& a = bbo.asset_to_buy(db);
   GRAPHENE_ASSERT( a.issuer == bbo.asset_to_buy_issuer,
      account_create_buyback_incorrect_issuer, "Incorrect asset issuer specified in buyback_account_options", ("asset", a)("bbo", bbo) );
   GRAPHENE_ASSERT( !a.buyback_account.valid(),
      account_create_buyback_already_exists, "Cannot create buyback for asset which already has buyback", ("asset", a)("bbo", bbo) );
   // TODO:  Replace with chain parameter #554
   GRAPHENE_ASSERT( bbo.markets.size() < GRAPHENE_DEFAULT_MAX_BUYBACK_MARKETS,
      account_create_buyback_too_many_markets, "Too many buyback markets", ("asset", a)("bbo", bbo) );
}

} }
