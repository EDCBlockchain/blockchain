#include <graphene/chain/protocol/cheque_ops.hpp>

namespace graphene { namespace chain {
   void cheque_create_operation::validate() const
   {
      FC_ASSERT(code.length() == 16, "Cheque code must be 16 symbols. Current length=${length}",("length", code.length()));
      FC_ASSERT(payee_amount.amount > 0, "Payee amount must be > 0. Current value: {a}",("a", payee_amount));
      FC_ASSERT(match_cheque_code(code));
   }

   void cheque_use_operation::validate() const {
      FC_ASSERT(code.length() == 16, "Cheque code must be 16 symbols. Current length=${length}",("length", code.length()));
   }

} } // namespace graphene::chain
