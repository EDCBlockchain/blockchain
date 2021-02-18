// see LICENSE.txt

#pragma once

#include <graphene/protocol/transaction.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {


/**
 *  @brief tracks the approval of a partially approved transaction 
 *  @ingroup object
 *  @ingroup protocol
 */
class proposal_object : public abstract_object<proposal_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = proposal_object_type;

      time_point_sec                expiration_time;
      optional<time_point_sec>      review_period_time;
      transaction                   proposed_transaction;
      flat_set<account_id_type>     required_active_approvals;
      flat_set<account_id_type>     available_active_approvals;
      flat_set<account_id_type>     required_owner_approvals;
      flat_set<account_id_type>     available_owner_approvals;
      flat_set<public_key_type>     available_key_approvals;

      bool is_authorized_to_execute(database& db)const;
};

/**
 *  @brief tracks all of the proposal objects that requrie approval of
 *  an individual account.   
 *
 *  @ingroup object
 *  @ingroup protocol
 *
 *  This is a secondary index on the proposal_index
 *
 *  @note the set of required approvals is constant
 */
class required_approval_index : public secondary_index
{
   public:
      virtual void object_inserted( const object& obj ) override;
      virtual void object_removed( const object& obj ) override;
      virtual void about_to_modify( const object& before ) override{};
      virtual void object_modified( const object& after  ) override{};

      void remove( account_id_type a, proposal_id_type p );

      map<account_id_type, set<proposal_id_type> > _account_to_proposals;
};

struct by_expiration{};
typedef boost::multi_index_container<
   proposal_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag< by_expiration >, member< proposal_object, time_point_sec, &proposal_object::expiration_time > >
   >
> proposal_multi_index_container;
typedef generic_index<proposal_object, proposal_multi_index_container> proposal_index;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::proposal_object)

FC_REFLECT_TYPENAME( graphene::chain::proposal_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::proposal_object )
