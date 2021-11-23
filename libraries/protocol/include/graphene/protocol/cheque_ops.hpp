#pragma once

#include <graphene/protocol/referral_classes.hpp>
#include <graphene/protocol/ext.hpp>

namespace graphene { namespace protocol {

   enum cheque_status
   {
      cheque_new,
      cheque_used,
      cheque_undo
   };

   /**
    * @ingroup operations
    */

   typedef static_variant<void_t, /* valid from */ fc::time_point_sec>::flat_set_type cheque_ext_type;

   struct cheque_create_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      std::string        code;
      account_id_type    account_id;
      asset              payee_amount; // amount per each user
      uint32_t           payee_count;
      fc::time_point_sec expiration_datetime;

      cheque_ext_type extensions;
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

} } // graphene::protocol

FC_REFLECT_ENUM( graphene::protocol::cheque_status,
                 (cheque_new)
                 (cheque_used)
                 (cheque_undo)
               )

FC_REFLECT( graphene::protocol::cheque_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::cheque_use_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::cheque_reverse_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::cheque_create_operation,
            (fee)(code)(account_id)(payee_amount)(payee_count)(expiration_datetime)(extensions))
FC_REFLECT( graphene::protocol::cheque_use_operation,
            (fee)(code)(account_id)(amount)(extensions))
FC_REFLECT( graphene::protocol::cheque_reverse_operation,
            (fee)(cheque_id)(account_id)(amount)(extensions))

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_use_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_reverse_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_use_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::cheque_reverse_operation )