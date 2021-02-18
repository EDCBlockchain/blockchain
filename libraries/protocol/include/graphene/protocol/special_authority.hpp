// see LICENSE.txt

#pragma once

#include <graphene/protocol/types.hpp>
#include <fc/reflect/reflect.hpp>

namespace graphene { namespace protocol {

struct no_special_authority {};

struct top_holders_special_authority
{
   asset_id_type asset;
   uint8_t       num_top_holders = 1;
};

typedef static_variant<
   no_special_authority,
   top_holders_special_authority
   > special_authority;

void validate_special_authority( const special_authority& auth );

} } // graphene::protocol

FC_REFLECT( graphene::protocol::no_special_authority, )
FC_REFLECT( graphene::protocol::top_holders_special_authority, (asset)(num_top_holders) )
FC_REFLECT_TYPENAME( graphene::protocol::special_authority )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::top_holders_special_authority )