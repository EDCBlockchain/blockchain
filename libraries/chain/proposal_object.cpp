// see LICENSE.txt

#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/proposal_object.hpp>

namespace graphene { namespace chain {

bool proposal_object::is_authorized_to_execute(database& db) const
{
   transaction_evaluation_state dry_run_eval(&db);

   try {
      verify_authority( proposed_transaction.operations, 
                        available_key_approvals,
                        [&]( account_id_type id ){ return &id(db).active; },
                        [&]( account_id_type id ){ return &id(db).owner;  },
                        db.get_global_properties().parameters.max_authority_depth,
                        true, /* allow committeee */
                        available_active_approvals,
                        available_owner_approvals );
   } 
   catch ( const fc::exception& e )
   {
      idump((available_active_approvals));
      wlog((e.to_detail_string()));
      return false;
   }
   return true;
}


void required_approval_index::object_inserted( const object& obj )
{
    assert( dynamic_cast<const proposal_object*>(&obj) );
    const proposal_object& p = static_cast<const proposal_object&>(obj);

    for( const auto& a : p.required_active_approvals )
       _account_to_proposals[a].insert( p.id );
    for( const auto& a : p.required_owner_approvals )
       _account_to_proposals[a].insert( p.id );
    for( const auto& a : p.available_active_approvals )
       _account_to_proposals[a].insert( p.id );
    for( const auto& a : p.available_owner_approvals )
       _account_to_proposals[a].insert( p.id );
}

void required_approval_index::remove( account_id_type a, proposal_id_type p )
{
    auto itr = _account_to_proposals.find(a);
    if( itr != _account_to_proposals.end() )
    {
        itr->second.erase( p );
        if( itr->second.empty() )
            _account_to_proposals.erase( itr->first );
    }
}

void required_approval_index::object_removed( const object& obj )
{
    assert( dynamic_cast<const proposal_object*>(&obj) );
    const proposal_object& p = static_cast<const proposal_object&>(obj);

    for( const auto& a : p.required_active_approvals )
       remove( a, p.id );
    for( const auto& a : p.required_owner_approvals )
       remove( a, p.id );
    for( const auto& a : p.available_active_approvals )
       remove( a, p.id );
    for( const auto& a : p.available_owner_approvals )
       remove( a, p.id );
}

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::proposal_object, (graphene::chain::object),
                    (expiration_time)(review_period_time)(proposed_transaction)(required_active_approvals)
                    (available_active_approvals)(required_owner_approvals)(available_owner_approvals)
                    (available_key_approvals) )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::proposal_object )
