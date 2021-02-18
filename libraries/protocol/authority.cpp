// see LICENSE.txt

#include <graphene/protocol/authority.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

void add_authority_accounts(
   flat_set<account_id_type>& result,
   const authority& a
   )
{
   for( auto& item : a.account_auths )
      result.insert( item.first );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::authority )
