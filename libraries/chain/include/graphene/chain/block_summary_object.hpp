// see LICENSE.txt

#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/object.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    *  @brief tracks minimal information about past blocks to implement TaPOS
    *  @ingroup object
    *
    *  When attempting to calculate the validity of a transaction we need to
    *  lookup a past block and check its block hash and the time it occurred
    *  so we can calculate whether the current transaction is valid and at
    *  what time it should expire.
    */
   class block_summary_object : public abstract_object<block_summary_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_block_summary_object_type;

         block_id_type      block_id;
   };

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::block_summary_object)

FC_REFLECT_TYPENAME( graphene::chain::block_summary_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::block_summary_object )
