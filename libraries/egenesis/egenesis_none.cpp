// see LICENSE.txt

#include <graphene/egenesis/egenesis.hpp>
#include <fc/io/raw.hpp>

namespace graphene { namespace egenesis {

using namespace graphene::chain;

chain_id_type get_egenesis_chain_id()
{
   return chain_id_type();
}

void compute_egenesis_json( std::string& result )
{
   result = "";
}

fc::sha256 get_egenesis_json_hash()
{
   return fc::sha256::hash( "" );
}

} }
