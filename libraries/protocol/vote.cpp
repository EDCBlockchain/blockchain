// see LICENSE.txt

#include <graphene/protocol/vote.hpp>

#include <fc/io/raw.hpp>

namespace fc
{

void to_variant(const graphene::protocol::vote_id_type& var, variant& vo, uint32_t max_depth)
{
   vo = string(var);
}

void from_variant(const variant& var, graphene::protocol::vote_id_type& vo, uint32_t max_depth)
{
   vo = graphene::protocol::vote_id_type(var.as_string());
}

} // fc

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::vote_id_type )