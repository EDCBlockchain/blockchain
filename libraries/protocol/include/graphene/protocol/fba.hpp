// see LICENSE.txt

#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include "../../../../chain/include/graphene/chain/types.hpp"

namespace graphene { namespace protocol {

struct fba_distribute_operation : public base_operation
{
   struct fee_parameters_type {};

   asset fee;   // always zero
   account_id_type account_id;
   // We use object_id_type because this is an implementation object,
   // and therefore is not known to the protocol library
   //object_id_type fba_id;
   chain::fba_accumulator_id_type fba_id;
   share_type amount;

   account_id_type fee_payer()const { return account_id; }
   void validate()const { FC_ASSERT( false ); }
   share_type calculate_fee(const fee_parameters_type& k)const { return 0; }
};

} }

FC_REFLECT( graphene::protocol::fba_distribute_operation::fee_parameters_type,  )
FC_REFLECT( graphene::protocol::fba_distribute_operation, (fee)(account_id)(fba_id)(amount) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fba_distribute_operation )
