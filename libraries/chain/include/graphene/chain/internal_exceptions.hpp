// see LICENSE.txt

#pragma once

#include <graphene/chain/exceptions.hpp>

#define GRAPHENE_DECLARE_INTERNAL_EXCEPTION( exc_name, seqnum, msg )  \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      internal_ ## exc_name,                                          \
      graphene::chain::internal_exception,                            \
      3990000 + seqnum                                                \
      )

#define GRAPHENE_IMPLEMENT_INTERNAL_EXCEPTION( exc_name, seqnum, msg )  \
   FC_IMPLEMENT_DERIVED_EXCEPTION(                                      \
      internal_ ## exc_name,                                          \
      graphene::chain::internal_exception,                            \
      3990000 + seqnum,                                               \
      msg                                                             \
      )

namespace graphene { namespace chain {

FC_DECLARE_DERIVED_EXCEPTION( internal_exception, graphene::chain::chain_exception, 3990000 )

GRAPHENE_DECLARE_INTERNAL_EXCEPTION( verify_auth_max_auth_exceeded, 1, "Exceeds max authority fan-out" )
GRAPHENE_DECLARE_INTERNAL_EXCEPTION( verify_auth_account_not_found, 2, "Auth account not found" )

} } // graphene::chain
