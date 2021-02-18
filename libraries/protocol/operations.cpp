// see LICENSE.txt

#include <graphene/protocol/operations.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

uint64_t base_operation::calculate_data_fee( uint64_t bytes, uint64_t price_per_kbyte )
{
   auto result = (fc::uint128_t(bytes) * price_per_kbyte) / 1024;
   FC_ASSERT( result <= GRAPHENE_MAX_SHARE_SUPPLY );
   return static_cast<uint64_t>(result);
}

void balance_claim_operation::validate()const
{
   FC_ASSERT( fee == asset() );
   FC_ASSERT( balance_owner_key != public_key_type() );
}

/**
 * @brief Used to validate operations in a polymorphic manner
 */
struct operation_validator
{
   typedef void result_type;
   template<typename T>
   void operator()( const T& v )const { v.validate(); }
};

struct operation_get_required_auth
{
   typedef void result_type;

   flat_set<account_id_type>& active;
   flat_set<account_id_type>& owner;
   vector<authority>&         other;

   operation_get_required_auth( flat_set<account_id_type>& a,
     flat_set<account_id_type>& own,
     vector<authority>&  oth ):active(a),owner(own),other(oth){}

   template<typename T>
   void operator()( const T& v )const 
   { 
      active.insert( v.fee_payer() );
      v.get_required_active_authorities( active ); 
      v.get_required_owner_authorities( owner ); 
      v.get_required_authorities( other );
   }
};

void operation_validate( const operation& op )
{
   op.visit( operation_validator() );
}

void operation_get_required_authorities( const operation& op, 
                                         flat_set<account_id_type>& active,
                                         flat_set<account_id_type>& owner,
                                         vector<authority>&  other )
{
   op.visit( operation_get_required_auth( active, owner, other ) );
}

} } // namespace graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::op_wrapper )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::eval_fund_dep_apply_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::eval_fund_dep_apply_object_fixed )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::market_address )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::market_addresses )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::dep_update_info )
