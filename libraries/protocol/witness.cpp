// see LICENSE.txt

#include <graphene/protocol/witness.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

void witness_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   FC_ASSERT(url.size() < GRAPHENE_MAX_URL_LENGTH );
}

void witness_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   if( new_url.valid() )
       FC_ASSERT(new_url->size() < GRAPHENE_MAX_URL_LENGTH );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::witness_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::witness_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::witness_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::witness_update_operation )