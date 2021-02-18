// see LICENSE.txt

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

namespace detail {

bool _is_authorized_asset(
   const database& d,
   const account_object& acct,
   const asset_object& asset_obj)
{
   if( acct.allowed_assets.valid() )
   {
      if( acct.allowed_assets->find( asset_obj.id ) == acct.allowed_assets->end() )
         return false;
      // must still pass other checks even if it is in allowed_assets
   }

   for( const auto id : acct.blacklisting_accounts )
   {
      if( asset_obj.options.blacklist_authorities.find(id) != asset_obj.options.blacklist_authorities.end() )
         return false;
   }

   if( d.head_block_time() > HARDFORK_415_TIME )
   {
      if( asset_obj.options.whitelist_authorities.size() == 0 )
         return true;
   }

   for( const auto id : acct.whitelisting_accounts )
   {
      if( asset_obj.options.whitelist_authorities.find(id) != asset_obj.options.whitelist_authorities.end() )
         return true;
   }

   return false;
}

} // detail

} } // graphene::chain
