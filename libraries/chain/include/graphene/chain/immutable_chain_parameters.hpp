// see LICENSE.txt

#pragma once

//#include <fc/reflect/reflect.hpp>
//#include <cstdint>

#include <graphene/chain/config.hpp>
#include <graphene/chain/types.hpp>

namespace graphene { namespace chain {

struct immutable_chain_parameters
{
   uint16_t min_committee_member_count = GRAPHENE_DEFAULT_MIN_COMMITTEE_MEMBER_COUNT;
   uint16_t min_witness_count = GRAPHENE_DEFAULT_MIN_WITNESS_COUNT;
   uint32_t num_special_accounts = 0;
   uint32_t num_special_assets = 0;
};

} } // graphene::chain

FC_REFLECT_TYPENAME( graphene::chain::immutable_chain_parameters )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::immutable_chain_parameters )
