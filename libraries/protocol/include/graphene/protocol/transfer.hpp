// see LICENSE.txt

#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/memo.hpp>

namespace graphene { namespace protocol {

   struct asset_ext_info
   {
      asset asst;
      std::string name;
      uint64_t precision = 0;
   };
   typedef static_variant<void_t, /* address */ std::string, asset_ext_info>::flat_set_type asset_ext_type;

   /**
    * @ingroup operations
    *
    * @brief Transfers an amount of one asset from one account to another
    *
    *  Fees are paid by the "from" account
    *
    *  @pre amount.amount > 0
    *  @pre fee.amount >= 0
    *  @pre from != to
    *  @post from account's balance will be reduced by fee and amount
    *  @post to account's balance will be increased by amount
    *  @return n/a
    */
   struct transfer_operation: public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; /// only required for large memos.
      };

      asset            fee;
      /// Account to transfer asset from
      account_id_type  from;
      /// Account to transfer asset to
      account_id_type  to;

      /// The amount of asset to transfer from @ref from to @ref to
      asset            amount;

      /// User provided data encrypted to the memo key of the "to" account
      optional<memo_data> memo;
      asset_ext_type extensions;

      account_id_type fee_payer()const { return from; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& k)const;
   };

   /**
    *  @class update_blind_transfer2_settings_operation
    *  @brief Allows update settings for blind_transfer2_operation
    *  @ingroup operations
    */
   struct update_blind_transfer2_settings_operation: public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset fee;
      asset blind_fee;

      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
      share_type      calculate_fee(const fee_parameters_type& k) const { return 0; };
   };

   /**
    * @ingroup operations
    *
    * @brief Transfers an amount of one asset from one account to another in blind mode
    *
    *  Fees are burned
    */
   struct blind_transfer2_operation: public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset            fee;

      account_id_type  from;
      account_id_type  to;
      asset            amount;

      optional<memo_data> memo;
      extensions_type extensions;

      account_id_type fee_payer() const { return from; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& k) const { return 0; };
   };

   /**
    *  @class override_transfer_operation
    *  @brief Allows the issuer of an asset to transfer an asset from any account to any account if they have override_authority
    *  @ingroup operations
    *
    *  @pre amount.asset_id->issuer == issuer
    *  @pre issuer != from  because this is pointless, use a normal transfer operation
    */
   struct override_transfer_operation: public base_operation
   {
      struct fee_parameters_type
      {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10; /// only required for large memos.
      };

      asset           fee;
      account_id_type issuer;
      /// Account to transfer asset from
      account_id_type from;
      /// Account to transfer asset to
      account_id_type to;
      /// The amount of asset to transfer from @ref from to @ref to
      asset amount;

      /// User provided data encrypted to the memo key of the "to" account
      optional<memo_data> memo;
      extensions_type   extensions;

      account_id_type fee_payer()const { return issuer; }
      void            validate()const;
      share_type      calculate_fee(const fee_parameters_type& k)const;
   };

}} // graphene::protocol

FC_REFLECT( graphene::protocol::asset_ext_info, (asst)(name)(precision) )

FC_REFLECT( graphene::protocol::transfer_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::transfer_operation, (fee)(from)(to)(amount)(memo)(extensions) )

FC_REFLECT( graphene::protocol::update_blind_transfer2_settings_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::update_blind_transfer2_settings_operation, (fee)(blind_fee)(extensions) )

FC_REFLECT( graphene::protocol::blind_transfer2_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::blind_transfer2_operation, (fee)(from)(to)(amount)(memo)(extensions) )

FC_REFLECT( graphene::protocol::override_transfer_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::override_transfer_operation, (fee)(issuer)(from)(to)(amount)(memo)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::asset_ext_info )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::transfer_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_blind_transfer2_settings_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::blind_transfer2_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::override_transfer_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::transfer_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::update_blind_transfer2_settings_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::blind_transfer2_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::override_transfer_operation )