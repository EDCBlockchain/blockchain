// see LICENSE.txt

#include <graphene/protocol/account.hpp>
#include <graphene/protocol/asset_ops.hpp>
#include <graphene/protocol/assert.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

bool account_name_eq_lit_predicate::validate()const
{
   return is_valid_name( name );
}

bool asset_symbol_eq_lit_predicate::validate()const
{
   return is_valid_symbol( symbol );
}

struct predicate_validator
{
   typedef void result_type;

   template<typename T>
   void operator()( const T& p )const
   {
      p.validate();
   }
};

void assert_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   for( const auto& item : predicates )
      item.visit( predicate_validator() );
}

/**
 * The fee for assert operations is proportional to their size,
 * but cheaper than a data fee because they require no storage
 */
share_type  assert_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee * predicates.size();
}

} }  // namespace graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::assert_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::assert_operation )
