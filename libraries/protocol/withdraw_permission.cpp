// see LICENSE.txt

#include <graphene/protocol/withdraw_permission.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

void withdraw_permission_update_operation::validate()const
{
   FC_ASSERT( withdrawal_limit.amount > 0 );
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdrawal_period_sec > 0 );
   FC_ASSERT( withdraw_from_account != authorized_account );
   FC_ASSERT( periods_until_expiration > 0 );
}

void withdraw_permission_claim_operation::validate()const
{
   FC_ASSERT( withdraw_to_account != withdraw_from_account );
   FC_ASSERT( amount_to_withdraw.amount > 0 );
   FC_ASSERT( fee.amount >= 0 );
}

share_type withdraw_permission_claim_operation::calculate_fee(const fee_parameters_type& k)const
{
   share_type core_fee_required = k.fee;
   if( memo )
      core_fee_required += calculate_data_fee( fc::raw::pack_size(memo), k.price_per_kbyte );
   return core_fee_required;
}

void withdraw_permission_create_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdraw_from_account != authorized_account );
   FC_ASSERT( withdrawal_limit.amount > 0 );
   //TODO: better bounds checking on these values
   FC_ASSERT( withdrawal_period_sec > 0 );
   FC_ASSERT( periods_until_expiration > 0 );
}

void withdraw_permission_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdraw_from_account != authorized_account );
}

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_update_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_claim_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_delete_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_update_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_claim_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::withdraw_permission_delete_operation )