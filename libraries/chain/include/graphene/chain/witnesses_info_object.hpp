#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>
#include <graphene/chain/database.hpp>

#include <iostream>

namespace graphene { namespace chain {

   /**
    * @class witnesses_info_object
    * @ingroup object
    * @ingroup protocol
    *
    * This object contains info about witnesses
    */
   class witnesses_info_object : public abstract_object<witnesses_info_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = witnesses_info_object_type;

      // all fee rewards between two maintenance times
      share_type witness_fees_reward_edc_amount;

      // all signed blocks between two MT
      int64_t all_blocks_reward_count = 0;

      flat_set<account_id_type> exc_accounts_blocks;
      flat_set<account_id_type> exc_accounts_fees;

      settings_id_type get_id() { return id; }
   };

}}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::witnesses_info_object)

FC_REFLECT_TYPENAME( graphene::chain::witnesses_info_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::witnesses_info_object )
