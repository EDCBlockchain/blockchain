// see LICENSE.txt

#pragma once

#include <graphene/protocol/types.hpp>

namespace graphene { namespace protocol {

struct buyback_account_options
{
   /**
    * The asset to buy.
    */
   asset_id_type       asset_to_buy;

   /**
    * Issuer of the asset.  Must sign the transaction, must match issuer
    * of specified asset.
    */
   account_id_type     asset_to_buy_issuer;

   /**
    * What assets the account is willing to buy with.
    * Other assets will just sit there since the account has null authority.
    */
   flat_set< asset_id_type > markets;
};

} }

FC_REFLECT( graphene::protocol::buyback_account_options, (asset_to_buy)(asset_to_buy_issuer)(markets) );

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::buyback_account_options )