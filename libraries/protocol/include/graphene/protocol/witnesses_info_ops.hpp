#pragma once

#include <graphene/protocol/referral_classes.hpp>
#include <graphene/protocol/ext.hpp>

namespace graphene { namespace protocol {
    
   /**
    * @ingroup operations
    */
   struct set_witness_exception_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      std::vector<account_id_type> exc_accounts_blocks;
      std::vector<account_id_type> exc_accounts_fees;
      bool exception_enabled = true;

      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::set_witness_exception_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::set_witness_exception_operation,
            (fee)(exc_accounts_blocks)(exc_accounts_fees)(exception_enabled)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_witness_exception_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_witness_exception_operation )
