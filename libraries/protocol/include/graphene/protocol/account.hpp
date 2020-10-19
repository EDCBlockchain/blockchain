/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <graphene/protocol/base.hpp>
#include <graphene/protocol/buyback.hpp>
#include <graphene/protocol/ext.hpp>
#include <graphene/protocol/special_authority.hpp>
#include <graphene/protocol/types.hpp>
#include <graphene/protocol/vote.hpp>
#include <regex>

namespace graphene { namespace protocol {

   const std::regex ADDRESS_REGEX = std::regex("EDC[(A-z)|(0-9)]{33}",  std::regex_constants::icase);
   const std::regex MARKET_ADDRESS_REGEX = std::regex("^edc[0-9a-zA-Z]{29}re$",  std::regex_constants::icase);

   bool is_valid_name( const string& s );
   bool is_cheap_name( const string& n );

   /// These are the fields which can be updated by the active authority.
   struct account_options
   {
      /// The memo key is the key this account will typically use to encrypt/sign transaction memos and other non-
      /// validated account activities. This field is here to prevent confusion if the active authority has zero or
      /// multiple keys in it.
      public_key_type  memo_key;
      /// If this field is set to an account ID other than GRAPHENE_PROXY_TO_SELF_ACCOUNT,
      /// then this account's votes will be ignored; its stake
      /// will be counted as voting for the referenced account's selected votes instead.
      account_id_type voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;

      /// The number of active witnesses this account votes the blockchain should appoint
      /// Must not exceed the actual number of witnesses voted for in @ref votes
      uint16_t num_witness = 0;
      /// The number of active committee members this account votes the blockchain should appoint
      /// Must not exceed the actual number of committee members voted for in @ref votes
      uint16_t num_committee = 0;
      /// This is the list of vote IDs this account votes for. The weight of these votes is determined by this
      /// account's balance of core asset.
      flat_set<vote_id_type> votes;
      extensions_type        extensions;

      void validate()const;
   };

   /**
    *  @ingroup operations
    */
   struct account_create_operation: public base_operation
   {
      struct ext
      {
         optional<void_t>            null_ext;
         optional<special_authority> owner_special_authority;
         optional<special_authority> active_special_authority;
         optional<buyback_account_options> buyback_options;
      };

      struct fee_parameters_type
      {
         uint64_t basic_fee       = 5*GRAPHENE_BLOCKCHAIN_PRECISION; ///< the cost to register the cheapest non-free account
         uint64_t premium_fee     = 2000*GRAPHENE_BLOCKCHAIN_PRECISION; ///< the cost to register the cheapest non-free account
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset           fee;
      /// This account pays the fee. Must be a lifetime member.
      account_id_type registrar;

      /// This account receives a portion of the fee split between registrar and referrer. Must be a member.
      account_id_type referrer;
      /// Of the fee split between registrar and referrer, this percentage goes to the referrer. The rest goes to the
      /// registrar.
      uint16_t        referrer_percent = 0;

      string          name;
      authority       owner;
      authority       active;

      account_options options;
      extension<ext> extensions;

      account_id_type fee_payer() const { return registrar; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& )const;

      void get_required_active_authorities( flat_set<account_id_type>& a )const
      {
         // registrar should be required anyway as it is the fee_payer(), but we insert it here just to be sure
         a.insert( registrar );
         if (extensions.value.buyback_options.valid()) {
            a.insert( extensions.value.buyback_options->asset_to_buy_issuer );
         }
      }
   };

   /**
    * @ingroup operations
    * @brief Update an existing account
    *
    * This operation is used to update an existing account. It can be used to update the authorities, or adjust the options on the account.
    */
   struct account_update_operation: public base_operation
   {
      struct ext
      {
         optional<void_t>            null_ext;
         optional<special_authority> owner_special_authority;
         optional<special_authority> active_special_authority;
      };

      struct fee_parameters_type
      {
         share_type fee             = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset fee;
      /// The account to update
      account_id_type account;

      /// New owner authority. If set, this operation requires owner authority to execute.
      optional<authority> owner;
      /// New active authority. This can be updated by the current active authority.
      optional<authority> active;

      /// New account options
      optional<account_options> new_options;
      extension<ext> extensions;

      account_id_type fee_payer()const { return account; }
      void       validate() const;
      share_type calculate_fee(const fee_parameters_type& k)const;

      bool is_owner_update()const
      { return owner || extensions.value.owner_special_authority.valid(); }

      void get_required_owner_authorities( flat_set<account_id_type>& a )const
      { if( is_owner_update() ) a.insert( account ); }

      void get_required_active_authorities( flat_set<account_id_type>& a )const
      { if( !is_owner_update() ) a.insert( account ); }
   };

   struct account_update_authorities_operation: public base_operation
   {
      struct fee_parameters_type {
         share_type fee = 0;
      };

      account_id_type fee_payer() const { return GRAPHENE_COMMITTEE_ACCOUNT; }
      void validate() const;
      share_type calculate_fee(const fee_parameters_type& k) const { return 0; }

      // the account to update
      account_id_type account;

      optional<authority> owner;
      optional<authority> active;
      optional<public_key_type> memo_key;

      asset fee;
      extensions_type extensions;
   };

   struct add_address_operation: public base_operation
   {
      struct fee_parameters_type {
         share_type fee = 0;
      };
      
      account_id_type fee_payer() const { return to_account; }
      void       validate() const {};
      share_type calculate_fee(const fee_parameters_type& k) const { return 0; }

      asset fee;
      account_id_type to_account;
      extensions_type extensions;
   };

   struct set_market_operation: public base_operation
   {
      struct fee_parameters_type {
         share_type fee = 0;
      };

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void       validate() const {};
      share_type calculate_fee(const fee_parameters_type& k) const { return 0; }

      asset fee;
      account_id_type to_account;
      bool enabled = false;

      extensions_type extensions;
   };

   /**
    * @brief This operation is used to whitelist and blacklist accounts, primarily for transacting in whitelisted assets
    * @ingroup operations
    *
    * Accounts can freely specify opinions about other accounts, in the form of either whitelisting or blacklisting
    * them. This information is used in chain validation only to determine whether an account is authorized to transact
    * in an asset type which enforces a whitelist, but third parties can use this information for other uses as well,
    * as long as it does not conflict with the use of whitelisted assets.
    *
    * An asset which enforces a whitelist specifies a list of accounts to maintain its whitelist, and a list of
    * accounts to maintain its blacklist. In order for a given account A to hold and transact in a whitelisted asset S,
    * A must be whitelisted by at least one of S's whitelist_authorities and blacklisted by none of S's
    * blacklist_authorities. If A receives a balance of S, and is later removed from the whitelist(s) which allowed it
    * to hold S, or added to any blacklist S specifies as authoritative, A's balance of S will be frozen until A's
    * authorization is reinstated.
    *
    * This operation requires authorizing_account's signature, but not account_to_list's. The fee is paid by
    * authorizing_account.
    */
   struct account_whitelist_operation: public base_operation
   {
      struct fee_parameters_type { share_type fee = 300000; };
      enum account_listing {
         no_listing = 0x0, ///< No opinion is specified about this account
         white_listed = 0x1, ///< This account is whitelisted, but not blacklisted
         black_listed = 0x2, ///< This account is blacklisted, but not whitelisted
         white_and_black_listed = white_listed | black_listed ///< This account is both whitelisted and blacklisted
      };

      /// Paid by authorizing_account
      asset           fee;
      /// The account which is specifying an opinion of another account
      account_id_type authorizing_account;
      /// The account being opined about
      account_id_type account_to_list;
      /// The new white and blacklist status of account_to_list, as determined by authorizing_account
      /// This is a bitfield using values defined in the account_listing enum
      uint8_t new_listing = no_listing;
      extensions_type extensions;

      account_id_type fee_payer() const { return authorizing_account; }
      void validate() const { FC_ASSERT( fee.amount >= 0 ); FC_ASSERT(new_listing < 0x4); }
   };
   
    struct account_restrict_operation: public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        enum account_action {
            restore = 0x1,
            restrict_in = 0x2,
            restrict_out = 0x4,
            restrict_all = restrict_in | restrict_out
        };
        
        asset               fee;
        account_id_type     target;
        uint8_t             action;
        extensions_type     extensions;
        
        account_id_type fee_payer()const { return GRAPHENE_COMMITTEE_ACCOUNT; }

        void validate() const
        {
            FC_ASSERT( target != GRAPHENE_COMMITTEE_ACCOUNT );
            FC_ASSERT(action);
            FC_ASSERT(action == 0x1 || action == 0x2 || action == 0x4 || action == 0x6);
        }
    };

    struct account_allow_referrals_operation: public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };


        enum account_action {
            allow = 0x1,
            disallow = 0x2,
        };
        
        asset               fee;
        account_id_type     target;
        uint8_t             action;
        extensions_type     extensions;
        
        account_id_type fee_payer() const { return GRAPHENE_COMMITTEE_ACCOUNT; }
        void validate() const { FC_ASSERT( target != GRAPHENE_COMMITTEE_ACCOUNT ); FC_ASSERT(action); FC_ASSERT(action == 0x1 || action == 0x2);}
    };

    struct set_online_time_operation: public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset                           fee;
        map<account_id_type, uint16_t>  online_info;
        extensions_type                 extensions;

        account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
        void            validate() const { };
    };

    struct set_verification_is_required_operation: public base_operation
    {
        struct fee_parameters_type { uint64_t fee = 0; };

        
        asset              fee;
        account_id_type    target;
        bool               verification_is_required;
        extensions_type    extensions;
        
        account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
        void            validate() const { };
    };

   /**
    * @brief Manage an account's membership status
    * @ingroup operations
    *
    * This operation is used to upgrade an account to a member, or renew its subscription. If an account which is an
    * unexpired annual subscription member publishes this operation with @ref upgrade_to_lifetime_member set to false,
    * the account's membership expiration date will be pushed backward one year. If a basic account publishes it with
    * @ref upgrade_to_lifetime_member set to false, the account will be upgraded to a subscription member with an
    * expiration date one year after the processing time of this operation.
    *
    * Any account may use this operation to become a lifetime member by setting @ref upgrade_to_lifetime_member to
    * true. Once an account has become a lifetime member, it may not use this operation anymore.
    */
   struct account_upgrade_operation: public base_operation
   {
      struct fee_parameters_type { 
         uint64_t membership_annual_fee   =  2000 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint64_t membership_lifetime_fee = 10000 * GRAPHENE_BLOCKCHAIN_PRECISION; ///< the cost to upgrade to a lifetime member
      };

      asset             fee;
      /// The account to upgrade; must not already be a lifetime member
      account_id_type   account_to_upgrade;
      /// If true, the account will be upgraded to a lifetime member; otherwise, it will add a year to the subscription
      bool              upgrade_to_lifetime_member = false;
      extensions_type   extensions;

      account_id_type fee_payer() const { return account_to_upgrade; }
      void            validate() const;
      share_type      calculate_fee(const fee_parameters_type& k) const;
   };

   /**
    * @brief transfers the account to another account while clearing the white list
    * @ingroup operations
    *
    * In theory an account can be transferred by simply updating the authorities, but that kind
    * of transfer lacks semantic meaning and is more often done to rotate keys without transferring
    * ownership.   This operation is used to indicate the legal transfer of title to this account and
    * a break in the operation history.
    *
    * The account_id's owner/active/voting/memo authority should be set to new_owner
    *
    * This operation will clear the account's whitelist statuses, but not the blacklist statuses.
    */
   struct account_transfer_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 500 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset           fee;
      account_id_type account_id;
      account_id_type new_owner;
      extensions_type extensions;

      account_id_type fee_payer() const { return account_id; }
      void            validate() const;
   };

   struct allow_create_addresses_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type account_id;
      bool            allow = true;
      extensions_type extensions;

      account_id_type fee_payer() const { return GRAPHENE_COMMITTEE_ACCOUNT; }
      void            validate() const { };
   };

   struct set_burning_mode_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset           fee;
      account_id_type account_id;
      bool            enabled = true;
      extensions_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
   };

   struct assets_update_fee_payer_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      fc::flat_set<asset_id_type> assets_to_update;
      asset_id_type               fee_payer_asset;

      extensions_type extensions;
      account_id_type fee_payer() const { return GRAPHENE_COMMITTEE_ACCOUNT; }
      void            validate() const { };
   };

   struct asset_update_exchange_rate_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      asset_id_type asset_to_update;
      price         core_exchange_rate;

      extensions_type extensions;
      account_id_type fee_payer() const { return GRAPHENE_COMMITTEE_ACCOUNT; }
      void            validate() const { };
   };

   typedef static_variant<void_t, uint32_t>::flat_set_type address_ext_type;

   struct create_market_address_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      account_id_type market_account_id;
      std::string     notes;

      //extensions_type extensions;
      address_ext_type extensions;

      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const;
   };

   struct limit_daily_ext_info
   {
      // transfers + blind_transfers
      share_type transfers_max_amount;
      bool       limit_cheques_enabled = false;
      share_type cheques_max_amount;
   };
   typedef static_variant<void_t, limit_daily_ext_info>::flat_set_type limit_daily_ext_type;

   struct account_edc_limit_daily_volume_operation: public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 0; };

      asset fee;

      account_id_type account_id;
      bool            limit_transfers_enabled = false;

      limit_daily_ext_type extensions;
      account_id_type fee_payer() const { return ALPHA_ACCOUNT_ID; }
      void            validate() const { };
   };

} } // graphene::protocol

FC_REFLECT_ENUM( graphene::protocol::account_whitelist_operation::account_listing,
                 (no_listing)(white_listed)(black_listed)(white_and_black_listed))
FC_REFLECT_ENUM( graphene::protocol::account_restrict_operation::account_action,
                 (restore)(restrict_in)(restrict_out)(restrict_all))
FC_REFLECT_ENUM( graphene::protocol::account_allow_referrals_operation::account_action,
                 (allow)(disallow))

FC_REFLECT_TYPENAME( graphene::protocol::address_ext_type )
FC_REFLECT( graphene::protocol::limit_daily_ext_info, (transfers_max_amount)(limit_cheques_enabled)(cheques_max_amount) )

FC_REFLECT( graphene::protocol::account_create_operation::ext, (null_ext)(owner_special_authority)(active_special_authority)(buyback_options) )
FC_REFLECT_TYPENAME(graphene::protocol::extension<graphene::protocol::account_create_operation::ext>)
FC_REFLECT( graphene::protocol::account_update_operation::ext, (null_ext)(owner_special_authority)(active_special_authority) )
FC_REFLECT_TYPENAME(graphene::protocol::extension<graphene::protocol::account_update_operation::ext>)

FC_REFLECT(graphene::protocol::account_options, (memo_key)(voting_account)(num_witness)(num_committee)(votes)(extensions))

FC_REFLECT( graphene::protocol::add_address_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_create_operation::fee_parameters_type, (basic_fee)(premium_fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::account_whitelist_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_restrict_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_allow_referrals_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::set_online_time_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::set_verification_is_required_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_update_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::account_update_authorities_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_upgrade_operation::fee_parameters_type, (membership_annual_fee)(membership_lifetime_fee) )
FC_REFLECT( graphene::protocol::account_transfer_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::allow_create_addresses_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::set_burning_mode_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::assets_update_fee_payer_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::asset_update_exchange_rate_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::set_market_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::create_market_address_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::account_edc_limit_daily_volume_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::account_create_operation,
            (fee)(registrar)
            (referrer)(referrer_percent)
            (name)(owner)(active)(options)(extensions) )
FC_REFLECT( graphene::protocol::account_update_operation,
            (fee)(account)(owner)(active)(new_options)(extensions) )
FC_REFLECT( graphene::protocol::account_update_authorities_operation,
            (account)(owner)(active)(memo_key)(fee)(extensions) )
FC_REFLECT( graphene::protocol::add_address_operation, (fee)(to_account)(extensions) )
FC_REFLECT( graphene::protocol::account_upgrade_operation,
            (fee)(account_to_upgrade)(upgrade_to_lifetime_member)(extensions) )
FC_REFLECT( graphene::protocol::account_whitelist_operation, (fee)(authorizing_account)(account_to_list)(new_listing)(extensions))
FC_REFLECT( graphene::protocol::account_restrict_operation, (fee)(target)(action)(extensions))
FC_REFLECT( graphene::protocol::account_allow_referrals_operation, (fee)(target)(action)(extensions))
FC_REFLECT( graphene::protocol::set_online_time_operation, (fee)(online_info)(extensions))
FC_REFLECT( graphene::protocol::set_verification_is_required_operation, (fee)(target)(verification_is_required)(extensions))
FC_REFLECT( graphene::protocol::account_transfer_operation, (fee)(account_id)(new_owner)(extensions) )
FC_REFLECT( graphene::protocol::allow_create_addresses_operation, (fee)(account_id)(allow)(extensions) )
FC_REFLECT( graphene::protocol::set_burning_mode_operation, (fee)(account_id)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::assets_update_fee_payer_operation, (fee)(assets_to_update)(fee_payer_asset)(extensions) )
FC_REFLECT( graphene::protocol::asset_update_exchange_rate_operation, (fee)(asset_to_update)(core_exchange_rate)(extensions) )
FC_REFLECT( graphene::protocol::set_market_operation, (fee)(to_account)(enabled)(extensions) )
FC_REFLECT( graphene::protocol::create_market_address_operation, (fee)(market_account_id)(notes)(extensions) )
FC_REFLECT( graphene::protocol::account_edc_limit_daily_volume_operation, (fee)(account_id)(limit_transfers_enabled)(extensions) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_options )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::limit_daily_ext_info )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::add_address_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_whitelist_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_restrict_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_allow_referrals_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_online_time_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_verification_is_required_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_update_authorities_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_upgrade_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_transfer_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::allow_create_addresses_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_burning_mode_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::assets_update_fee_payer_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::asset_update_exchange_rate_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_market_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::create_market_address_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_edc_limit_daily_volume_operation::fee_parameters_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_update_authorities_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::add_address_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_upgrade_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_whitelist_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_restrict_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_allow_referrals_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_online_time_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_verification_is_required_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_transfer_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::allow_create_addresses_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_burning_mode_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::assets_update_fee_payer_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::asset_update_exchange_rate_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::set_market_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::create_market_address_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::account_edc_limit_daily_volume_operation )