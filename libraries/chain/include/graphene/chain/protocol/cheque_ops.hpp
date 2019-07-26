#pragma once

#include <graphene/chain/protocol/referral_classes.hpp>
#include <graphene/chain/protocol/ext.hpp>
#include <regex>

namespace graphene { namespace chain {

   enum cheque_status
   {
      cheque_new,
      cheque_used,
      cheque_undo
   };

   inline bool match_cheque_code(const std::string& cheque_code) {
      return std::regex_match(cheque_code.c_str(), std::regex("[a-zA-Z0-9]+"));
   }

   /**
    * @ingroup operations
    */
   struct cheque_create_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      std::string        code;
      account_id_type    account_id;
      asset              payee_amount; // amount per each user
      uint32_t           payee_count;
      fc::time_point_sec expiration_datetime;

      extensions_type extensions;
      account_id_type fee_payer() const { return account_id; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // cheque_create_operation

   struct cheque_use_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;

      std::string     code;
      account_id_type account_id;
      asset           amount;
      extensions_type extensions;

      account_id_type fee_payer() const { return account_id; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // cheque_use_operation

   struct cheque_reverse_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      cheque_id_type  cheque_id;
      account_id_type account_id;
      asset           amount;
      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& params) const { return 0; }

   }; // cheque_reverse_operation

} } // graphene::chain

FC_REFLECT( graphene::chain::cheque_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::cheque_create_operation,
            (fee)(code)(account_id)(payee_amount)(payee_count)(expiration_datetime)(extensions))

FC_REFLECT( graphene::chain::cheque_use_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::cheque_use_operation,
            (fee)(code)(account_id)(amount)(extensions))

FC_REFLECT( graphene::chain::cheque_reverse_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::cheque_reverse_operation,
            (fee)(cheque_id)(account_id)(amount)(extensions))

FC_REFLECT_ENUM( graphene::chain::cheque_status,
                 (cheque_new)
                 (cheque_used)
                 (cheque_undo)
               )