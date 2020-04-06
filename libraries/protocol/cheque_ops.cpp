#include <graphene/protocol/cheque_ops.hpp>
#include <fc/io/raw.hpp>
#include <regex>

namespace graphene { namespace protocol {

   inline bool match_cheque_code(const std::string& cheque_code) {
      return std::regex_match(cheque_code.c_str(), std::regex("[a-zA-Z0-9]+"));
   }

   void cheque_create_operation::validate() const
   {
      FC_ASSERT(code.length() == 16, "Cheque code must be 16 symbols. Current length=${length}",("length", code.length()));
      FC_ASSERT(payee_amount.amount > 0, "Payee amount must be > 0. Current value: ${a}",("a", payee_amount));
      FC_ASSERT(match_cheque_code(code));
   }

   void cheque_use_operation::validate() const {
      FC_ASSERT(code.length() == 16, "Cheque code must be 16 symbols. Current length=${length}",("length", code.length()));
   }

} } // namespace graphene::chain

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_use_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_reverse_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_use_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_reverse_operation )