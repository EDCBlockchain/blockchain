// see LICENSE.txt

#pragma once
#include <graphene/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

class witness_schedule_object;

class witness_schedule_object : public graphene::db::abstract_object<witness_schedule_object>
{
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_witness_schedule_object_type;

      vector< witness_id_type > current_shuffled_witnesses;
};

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::witness_schedule_object)

FC_REFLECT_TYPENAME( graphene::chain::witness_schedule_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::witness_schedule_object )
