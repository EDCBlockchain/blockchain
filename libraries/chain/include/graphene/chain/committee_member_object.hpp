// see LICENSE.txt

#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/vote.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    *  @brief tracks information about a committee_member account.
    *  @ingroup object
    *
    *  A committee_member is responsible for setting blockchain parameters and has
    *  dynamic multi-sig control over the committee account.  The current set of
    *  active committee_members has control.
    *
    *  committee_members were separated into a separate object to make iterating over
    *  the set of committee_member easy.
    */
   class committee_member_object : public abstract_object<committee_member_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = committee_member_object_type;

         account_id_type  committee_member_account;
         vote_id_type     vote_id;
         uint64_t         total_votes = 0;
         string           url;
   };

   struct by_account;
   struct by_vote_id;
   using committee_member_multi_index_type = multi_index_container<
      committee_member_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<committee_member_object, account_id_type, &committee_member_object::committee_member_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<committee_member_object, vote_id_type, &committee_member_object::vote_id>
         >
      >
   >;
   using committee_member_index = generic_index<committee_member_object, committee_member_multi_index_type>;
} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::committee_member_object)

FC_REFLECT_TYPENAME( graphene::chain::committee_member_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::committee_member_object )
