// see LICENSE.txt

#include <graphene/protocol/special_authority.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

struct special_authority_validate_visitor
{
   typedef void result_type;

   void operator()( const no_special_authority& a ) {}

   void operator()( const top_holders_special_authority& a )
   {
      FC_ASSERT( a.num_top_holders > 0 );
   }
};

void validate_special_authority( const special_authority& a )
{
   special_authority_validate_visitor vtor;
   a.visit( vtor );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::top_holders_special_authority )