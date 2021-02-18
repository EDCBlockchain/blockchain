// see LICENSE.txt

#include <graphene/chain/special_authority_evaluation.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

struct special_authority_evaluate_visitor
{
   typedef void result_type;

   special_authority_evaluate_visitor( const database& d ) : db(d) {}

   void operator()( const no_special_authority& a ) {}

   void operator()( const top_holders_special_authority& a )
   {
      db.get(a.asset);     // require asset to exist
   }

   const database& db;
};

void evaluate_special_authority( const database& db, const special_authority& a )
{
   special_authority_evaluate_visitor vtor( db );
   a.visit( vtor );
}

} } // graphene::chain
