// see LICENSE.txt

#pragma once

#include <fc/exception/exception.hpp>

#define GRAPHENE_ASSERT( expr, exc_type, FORMAT, ... )                \
   FC_MULTILINE_MACRO_BEGIN                                           \
   if( !(expr) )                                                      \
      FC_THROW_EXCEPTION( exc_type, FORMAT, __VA_ARGS__ );            \
   FC_MULTILINE_MACRO_END

namespace graphene { namespace protocol {

   FC_DECLARE_EXCEPTION( protocol_exception, 4000000 )

   FC_DECLARE_DERIVED_EXCEPTION( transaction_exception,      protocol_exception, 4010000 )

   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_active_auth,     transaction_exception, 4010001 )
   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_owner_auth,      transaction_exception, 4010002 )
   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_other_auth,      transaction_exception, 4010003 )
   FC_DECLARE_DERIVED_EXCEPTION( tx_irrelevant_sig,          transaction_exception, 4010004 )
   FC_DECLARE_DERIVED_EXCEPTION( tx_duplicate_sig,           transaction_exception, 4010005 )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_committee_approval, transaction_exception, 4010006 )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_fee,           transaction_exception, 4010007 )

} } // graphene::protocol
