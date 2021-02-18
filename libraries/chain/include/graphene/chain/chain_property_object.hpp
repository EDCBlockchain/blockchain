// see LICENSE.txt

#pragma once

#include <graphene/chain/immutable_chain_parameters.hpp>

namespace graphene { namespace chain {

/**
 * Contains invariants which are set at genesis and never changed.
 */
class chain_property_object : public abstract_object<chain_property_object>
{
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id  = impl_chain_property_object_type;

      chain_id_type chain_id;
      immutable_chain_parameters immutable_parameters;
};

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::chain_property_object)

FC_REFLECT_TYPENAME( graphene::chain::chain_property_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::chain_property_object )
