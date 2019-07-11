#pragma once

#include <graphene/chain/protocol/referral_classes.hpp>
#include <graphene/chain/protocol/ext.hpp>
#include <regex>

namespace graphene { namespace chain {

   enum receipt_statuses {
      receipt_new,
      receipt_used,
      receipt_undo
   };

   inline bool match_receipt_code(const std::string& receipt_code)
   {
      std::regex rx("[a-zA-Z0-9]+");
      return std::regex_match(receipt_code.c_str(),rx);
   }
   /**
    * @ingroup operations
    */
   struct receipt_create_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      std::string        receipt_code;
      account_id_type    maker;
      asset              amount;
      fc::time_point_sec expiration_datetime;

      extensions_type extensions;
      account_id_type fee_payer() const { return maker; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // receipt_create_operation

   struct receipt_use_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      std::string     receipt_code;
      account_id_type taker;
      extensions_type extensions;

      account_id_type fee_payer() const { return taker; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // receipt_create_operation

   struct receipt_undo_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      std::string     receipt_code;
      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // receipt_undo_operation

} } // graphene::chain

FC_REFLECT( graphene::chain::receipt_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::receipt_create_operation,
            (fee)(receipt_code)(maker)(amount)(expiration_datetime)(extensions))

FC_REFLECT( graphene::chain::receipt_use_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::receipt_use_operation,
            (fee)(receipt_code)(taker)(extensions))

FC_REFLECT( graphene::chain::receipt_undo_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::receipt_undo_operation,
            (fee)(receipt_code)(extensions))

FC_REFLECT_ENUM( graphene::chain::receipt_statuses,
                 (receipt_new)
                 (receipt_used)
                 (receipt_undo)
)