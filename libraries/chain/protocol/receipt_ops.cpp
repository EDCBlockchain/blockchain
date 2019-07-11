#include <graphene/chain/protocol/receipt_ops.hpp>

namespace graphene { namespace chain {
   void receipt_create_operation::validate() const
   {
      FC_ASSERT(receipt_code.length() == 16, "Receipt code must be 16 symbols. Current length=${length}",("length", receipt_code.length()));
      FC_ASSERT(amount.amount > 0, "amount must be > 0. Current value: {a}",("a", amount));
   }

   void receipt_use_operation::validate() const
   {
      FC_ASSERT(receipt_code.length() == 16, "Receipt code must be 16 symbols. Current length=${length}",("length", receipt_code.length()));
   }

   void receipt_undo_operation::validate() const
   {
      FC_ASSERT(receipt_code.length() == 16, "Receipt code must be 16 symbols. Current length=${length}",("length", receipt_code.length()));
   }

} } // namespace graphene::chain
