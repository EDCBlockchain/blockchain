// see LICENSE.txt

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/settings_object.hpp>
#include <graphene/chain/internal_exceptions.hpp>
#include <graphene/chain/special_authority.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <iostream>
#include <algorithm>

namespace graphene { namespace chain {

void verify_authority_accounts( const database& db, const authority& a )
{
   const auto& chain_params = db.get_global_properties().parameters;
   GRAPHENE_ASSERT(
      a.num_auths() <= chain_params.maximum_authority_membership,
      internal_verify_auth_max_auth_exceeded,
      "Maximum authority membership exceeded" );
   for( const auto& acnt : a.account_auths )
   {
      GRAPHENE_ASSERT( db.find_object( acnt.first ) != nullptr,
         internal_verify_auth_account_not_found,
         "Account ${a} specified in authority does not exist",
         ("a", acnt.first) );
   }
}

void check_accounts_usage(database& _db, set<account_id_type> new_accs, set<public_key_type> new_keys)
{
   for( auto key : new_keys)
   {
      int count = 0;
      const auto& idx = _db.get_index_type<account_index>();
      const auto& aidx = dynamic_cast<const primary_index<account_index>&>(idx);
      const auto& refs = aidx.get_secondary_index<graphene::chain::account_member_index>();
      auto itr = refs.account_to_key_memberships.find(key);

      // wtf: no more than 3 equal keys for one account?
      if( itr != refs.account_to_key_memberships.end() )
      {
         for( auto item : itr->second )
         {
            (void)item;

            count++;
            FC_ASSERT(count < 3); // can be 0 - 12 without assert
         }
      }
   }

   const auto& idx = _db.get_index_type<account_index>();
   const auto& aidx = dynamic_cast<const primary_index<account_index>&>(idx);
   const auto& refs = aidx.get_secondary_index<graphene::chain::account_member_index>();

   for( auto acc : new_accs)
   {
      int count = 0;
      auto itr = refs.account_to_account_memberships.find(acc);

      if( itr != refs.account_to_account_memberships.end() )
      {
         for( auto item : itr->second )
         {
            (void)item;
            count++;
            FC_ASSERT(count < 3);
         }
      }
   }
}

void verify_account_votes( const database& db, const account_options& options )
{
   // ensure account's votes satisfy requirements
   // NB only the part of vote checking that requires chain state is here,
   // the rest occurs in account_options::validate()

   const auto& gpo = db.get_global_properties();
   const auto& chain_params = gpo.parameters;

   FC_ASSERT( options.num_witness <= chain_params.maximum_witness_count,
              "Voted for more witnesses than currently allowed (${c})", ("c", chain_params.maximum_witness_count) );
   FC_ASSERT( options.num_committee <= chain_params.maximum_committee_count,
              "Voted for more committee members than currently allowed (${c})", ("c", chain_params.maximum_committee_count) );

   uint32_t max_vote_id = gpo.next_available_vote_id;
   bool has_worker_votes = false;
   for( auto id : options.votes )
   {
      FC_ASSERT( id < max_vote_id );
      has_worker_votes |= (id.type() == vote_id_type::worker);
   }

   if( has_worker_votes && (db.head_block_time() >= HARDFORK_607_TIME) )
   {
      const auto& against_worker_idx = db.get_index_type<worker_index>().indices().get<by_vote_against>();
      for( auto id : options.votes )
      {
         if( id.type() == vote_id_type::worker )
         {
            FC_ASSERT( against_worker_idx.find( id ) == against_worker_idx.end() );
         }
      }
   }

}

//////////////////////////////////////////////////////////////////////////////////////

void_result account_create_evaluator::do_evaluate( const account_create_operation& op )
{ try {
   database& d = db();
   if( d.head_block_time() < HARDFORK_516_TIME )
   {
      FC_ASSERT( !op.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.active_special_authority.valid() );
   }
   if( d.head_block_time() < HARDFORK_599_TIME )
   {
      FC_ASSERT( !op.extensions.value.null_ext.valid() );
      FC_ASSERT( !op.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.active_special_authority.valid() );
      FC_ASSERT( !op.extensions.value.buyback_options.valid() );
   }

   if (d.head_block_time() > HARDFORK_617_TIME)
   {
      set<account_id_type> accs;
      set<public_key_type> keys;
      auto va = op.owner.get_accounts();
      auto vk = op.owner.get_keys();
      accs.insert(va.begin(), va.end());
      keys.insert(vk.begin(), vk.end());
      va = op.active.get_accounts();
      vk = op.active.get_keys();
      accs.insert(va.begin(), va.end());
      keys.insert(vk.begin(), vk.end());

      if (!db().registrar_mode_is_enabled())
      {
         check_accounts_usage(d, accs, keys);

         auto& p = d.get_account_properties();
         auto prop_itr = p.accounts_properties.find(op.registrar);

         FC_ASSERT(prop_itr != p.accounts_properties.end());
         FC_ASSERT(prop_itr->second.can_be_registrar);
      }
   }

   FC_ASSERT( d.find_object(op.options.voting_account), "Invalid proxy account specified." );
//   FC_ASSERT( fee_paying_account->is_lifetime_member(), "Only Lifetime members may register an account." );
//   FC_ASSERT( op.referrer(d).is_member(d.head_block_time()), "The referrer must be either a lifetime or annual subscriber." );

   try
   {
      verify_authority_accounts( d, op.owner );
      verify_authority_accounts( d, op.active );
   }
   GRAPHENE_RECODE_EXC( internal_verify_auth_max_auth_exceeded, account_create_max_auth_exceeded )
   GRAPHENE_RECODE_EXC( internal_verify_auth_account_not_found, account_create_auth_account_not_found )

   if( op.extensions.value.owner_special_authority.valid() )
      evaluate_special_authority( d, *op.extensions.value.owner_special_authority );
   if( op.extensions.value.active_special_authority.valid() )
      evaluate_special_authority( d, *op.extensions.value.active_special_authority );
   if( op.extensions.value.buyback_options.valid() )
      evaluate_buyback_account_options( d, *op.extensions.value.buyback_options );
   verify_account_votes( d, op.options );

   auto& acnt_indx = d.get_index_type<account_index>();
   if( op.name.size() )
   {
      auto current_account_itr = acnt_indx.indices().get<by_name>().find( op.name );
      FC_ASSERT( current_account_itr == acnt_indx.indices().get<by_name>().end(),
                 "Account '${a}' already exists.", ("a",op.name) );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type account_create_evaluator::do_apply( const account_create_operation& o )
{ try {

   database& d = db();

   uint16_t referrer_percent = o.referrer_percent;
   bool has_small_percent = (
         (db().head_block_time() <= HARDFORK_453_TIME)
      && (o.referrer != o.registrar  )
      && (o.referrer_percent != 0    )
      && (o.referrer_percent <= 0x100)
      );
   if( has_small_percent )
   {
      if( referrer_percent >= 100 )
      {
         wlog( "between 100% and 0x100%:  ${o}", ("o", o) );
      }
      referrer_percent = referrer_percent*100;
      if( referrer_percent > GRAPHENE_100_PERCENT )
         referrer_percent = GRAPHENE_100_PERCENT;
   }

   const auto& new_acnt_object = db().create<account_object>( [&]( account_object& obj )
   {
      obj.registrar = o.registrar;
      obj.referrer = o.referrer;
      obj.lifetime_referrer = o.referrer(db()).lifetime_referrer;

      auto& params = db().get_global_properties().parameters;
      obj.network_fee_percentage = params.network_percent_of_fee;
      obj.lifetime_referrer_fee_percentage = params.lifetime_referrer_percent_of_fee;
      obj.referrer_rewards_percentage = referrer_percent;

      bool is_market_account = std::regex_match(o.name, MARKET_ADDRESS_REGEX);

      // verification
      if ( (d.head_block_time() >= HARDFORK_629_TIME) && (d.head_block_time() < HARDFORK_630_TIME) )
      {
         // if it is not a market address then should be verified
         if (!is_market_account) {
            obj.verification_is_required = true;
         }
      }

      if (d.head_block_time() >= HARDFORK_642_TIME) {
         obj.deposits_autorenewal_enabled = false;
      }

      obj.is_market_account = is_market_account;
      obj.name              = o.name;
      obj.register_datetime = d.head_block_time();
      obj.owner             = o.owner;
      obj.active            = o.active;
      obj.options           = o.options;
      obj.statistics = db().create<account_statistics_object>([&](account_statistics_object& s){s.owner = obj.id;}).id;

      if( o.extensions.value.owner_special_authority.valid() )
         obj.owner_special_authority = *(o.extensions.value.owner_special_authority);
      if( o.extensions.value.active_special_authority.valid() )
         obj.active_special_authority = *(o.extensions.value.active_special_authority);
      if( o.extensions.value.buyback_options.valid() )
      {
         obj.allowed_assets = o.extensions.value.buyback_options->markets;
         obj.allowed_assets->emplace( o.extensions.value.buyback_options->asset_to_buy );
      }
   });

   if( has_small_percent )
   {
      wlog( "Account affected by #453 registered in block ${n}:  ${na} reg=${reg} ref=${ref}:${refp} ltr=${ltr}:${ltrp}",
         ("n", db().head_block_num()) ("na", new_acnt_object.id)
         ("reg", o.registrar) ("ref", o.referrer) ("ltr", new_acnt_object.lifetime_referrer)
         ("refp", new_acnt_object.referrer_rewards_percentage) ("ltrp", new_acnt_object.lifetime_referrer_fee_percentage) );
      wlog( "Affected account object is ${o}", ("o", new_acnt_object) );
   }

   const auto& dynamic_properties = db().get_dynamic_global_properties();
   db().modify(dynamic_properties, [](dynamic_global_property_object& p) {
      ++p.accounts_registered_this_interval;
   });

   const auto& global_properties = db().get_global_properties();
   if( dynamic_properties.accounts_registered_this_interval %
       global_properties.parameters.accounts_per_fee_scale == 0 )
      db().modify(global_properties, [](global_property_object& p) {
         p.parameters.get_mutable_fees().get<account_create_operation>().basic_fee <<= p.parameters.account_fee_scale_bitshifts;
      });

   if ( o.extensions.value.owner_special_authority.valid()
        || o.extensions.value.active_special_authority.valid() )
   {
      db().create< special_authority_object >( [&]( special_authority_object& sa )
      {
         sa.account = new_acnt_object.id;
      } );
   }

   if( o.extensions.value.buyback_options.valid() )
   {
      asset_id_type asset_to_buy = o.extensions.value.buyback_options->asset_to_buy;

      d.create< buyback_object >( [&]( buyback_object& bo )
      {
         bo.asset_to_buy = asset_to_buy;
      } );

      d.modify( asset_to_buy(d), [&]( asset_object& a )
      {
         a.buyback_account = new_acnt_object.id;
      } );
   }

   return new_acnt_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_update_evaluator::do_evaluate( const account_update_operation& o )
{ try {
   database& d = db();
   if( d.head_block_time() < HARDFORK_516_TIME )
   {
      FC_ASSERT( !o.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !o.extensions.value.active_special_authority.valid() );
   }
   if( d.head_block_time() < HARDFORK_599_TIME )
   {
      FC_ASSERT( !o.extensions.value.null_ext.valid() );
      FC_ASSERT( !o.extensions.value.owner_special_authority.valid() );
      FC_ASSERT( !o.extensions.value.active_special_authority.valid() );
   }

   if (d.head_block_time() > HARDFORK_617_TIME)
   {
      set<account_id_type> accs;
      set<public_key_type> keys;
      if (o.owner)
      {
         auto va = (*o.owner).get_accounts();
         auto vk = (*o.owner).get_keys();
         accs.insert(va.begin(), va.end());
         keys.insert(vk.begin(), vk.end());
      }
      if (o.active)
      {
         auto va = (*o.active).get_accounts();
         auto vk = (*o.active).get_keys();
         accs.insert(va.begin(), va.end());
         keys.insert(vk.begin(), vk.end());
      }

      if (!db().registrar_mode_is_enabled()) {
         check_accounts_usage(d, accs, keys);
      }
   }

   try
   {
      if( o.owner )  verify_authority_accounts( d, *o.owner );
      if( o.active ) verify_authority_accounts( d, *o.active );
   }
   GRAPHENE_RECODE_EXC( internal_verify_auth_max_auth_exceeded, account_update_max_auth_exceeded )
   GRAPHENE_RECODE_EXC( internal_verify_auth_account_not_found, account_update_auth_account_not_found )

   if( o.extensions.value.owner_special_authority.valid() )
      evaluate_special_authority( d, *o.extensions.value.owner_special_authority );
   if( o.extensions.value.active_special_authority.valid() )
      evaluate_special_authority( d, *o.extensions.value.active_special_authority );

   acnt = &o.account(d);

   if( o.new_options.valid() )
      verify_account_votes( d, *o.new_options );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_update_evaluator::do_apply( const account_update_operation& o )
{ try {
   database& d = db();
   bool sa_before, sa_after;

   d.modify( *acnt, [&](account_object& a){
      if( o.owner )
      {
         a.owner = *o.owner;
         a.top_n_control_flags = 0;
      }
      if( o.active )
      {
         a.active = *o.active;
         a.top_n_control_flags = 0;
      }
      if( o.new_options ) a.options = *o.new_options;
      sa_before = a.has_special_authority();
      if( o.extensions.value.owner_special_authority.valid() )
      {
         a.owner_special_authority = *(o.extensions.value.owner_special_authority);
         a.top_n_control_flags = 0;
      }
      if( o.extensions.value.active_special_authority.valid() )
      {
         a.active_special_authority = *(o.extensions.value.active_special_authority);
         a.top_n_control_flags = 0;
      }
      sa_after = a.has_special_authority();
   });

   if( sa_before & (!sa_after) )
   {
      const auto& sa_idx = d.get_index_type< special_authority_index >().indices().get<by_account>();
      auto sa_it = sa_idx.find( o.account );
      assert( sa_it != sa_idx.end() );
      d.remove( *sa_it );
   }
   else if( (!sa_before) & sa_after )
   {
      d.create< special_authority_object >( [&]( special_authority_object& sa )
      {
         sa.account = o.account;
      } );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_update_authorities_evaluator::do_evaluate(const account_update_authorities_operation& o)
{ try {
   database& d = db();

   set<account_id_type> accs;
   set<public_key_type> keys;
   if (o.owner)
   {
      auto va = (*o.owner).get_accounts();
      auto vk = (*o.owner).get_keys();
      accs.insert(va.begin(), va.end());
      keys.insert(vk.begin(), vk.end());
   }
   if (o.active)
   {
      auto va = (*o.active).get_accounts();
      auto vk = (*o.active).get_keys();
      accs.insert(va.begin(), va.end());
      keys.insert(vk.begin(), vk.end());
   }

   if (!db().registrar_mode_is_enabled()) {
      check_accounts_usage(d, accs, keys);
   }

   try
   {
      if (o.owner) {
         verify_authority_accounts(d, *o.owner);
      }
      if (o.active) {
         verify_authority_accounts(d, *o.active);
      }
   }
   GRAPHENE_RECODE_EXC(internal_verify_auth_max_auth_exceeded, account_update_max_auth_exceeded)
   GRAPHENE_RECODE_EXC(internal_verify_auth_account_not_found, account_update_auth_account_not_found)

   account_ptr = &o.account(d);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_update_authorities_evaluator::do_apply(const account_update_authorities_operation& o)
{ try {

   database& d = db();

   d.modify(*account_ptr, [&](account_object& a)
   {
      if (o.owner)
      {
         a.owner = *o.owner;
         a.top_n_control_flags = 0;
      }
      if (o.active)
      {
         a.active = *o.active;
         a.top_n_control_flags = 0;
      }
      if (o.memo_key) {
         a.options.memo_key = *o.memo_key;
      }
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result add_address_evaluator::do_evaluate(const add_address_operation& o) 
{
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(o.to_account);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", o.to_account));

   account_ptr = &*itr;

   FC_ASSERT(account_ptr->can_create_addresses, "Account ${a} can't create addresses (restricted by committee)!", ("a", account_ptr->name));

   return void_result();
}

void_result add_address_evaluator::do_apply(const add_address_operation& o)
{ try {

   if (account_ptr)
   {
      database& d = db();

      const address& addr = d.get_address();

      d.modify(*account_ptr, [&](account_object& obj)
      {
         obj.last_generated_address = addr;
         obj.addresses.emplace_back(addr);
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_whitelist_evaluator::do_evaluate(const account_whitelist_operation& o)
{ try {
   database& d = db();

   listed_account = &o.account_to_list(d);
   if( !d.get_global_properties().parameters.allow_non_member_whitelists )
      FC_ASSERT(o.authorizing_account(d).is_lifetime_member());

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result account_whitelist_evaluator::do_apply(const account_whitelist_operation& o)
{ try {
   database& d = db();

   d.modify(*listed_account, [&o](account_object& a) {
      if( o.new_listing & o.white_listed )
         a.whitelisting_accounts.insert(o.authorizing_account);
      else
         a.whitelisting_accounts.erase(o.authorizing_account);

      if( o.new_listing & o.black_listed )
         a.blacklisting_accounts.insert(o.authorizing_account);
      else
         a.blacklisting_accounts.erase(o.authorizing_account);
   });

   /** for tracking purposes only, this state is not needed to evaluate */
   d.modify( o.authorizing_account(d), [&]( account_object& a ) {
     if( o.new_listing & o.white_listed )
        a.whitelisted_accounts.insert( o.account_to_list );
     else
        a.whitelisted_accounts.erase( o.account_to_list );

     if( o.new_listing & o.black_listed )
        a.blacklisted_accounts.insert( o.account_to_list );
     else
        a.blacklisted_accounts.erase( o.account_to_list );
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_upgrade_evaluator::do_evaluate(const account_upgrade_evaluator::operation_type& o)
{ try {
   database& d = db();

   account = &d.get(o.account_to_upgrade);
   FC_ASSERT(!account->is_lifetime_member());

   return {};
//} FC_CAPTURE_AND_RETHROW( (o) ) }
} FC_RETHROW_EXCEPTIONS( error, "Unable to upgrade account '${a}'", ("a",o.account_to_upgrade(db()).name) ) }

void_result account_upgrade_evaluator::do_apply(const account_upgrade_evaluator::operation_type& o)
{ try {
   database& d = db();

   d.modify(*account, [&](account_object& a) {
      if( o.upgrade_to_lifetime_member )
      {
         // Upgrade to lifetime member. I don't care what the account was before.
         a.statistics(d).process_fees(a, d);
         a.membership_expiration_date = time_point_sec::maximum();
         //a.referrer =
         a.registrar = a.lifetime_referrer = a.get_id();
         a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - a.network_fee_percentage;
      } else if( a.is_annual_member(d.head_block_time()) ) {
         // Renew an annual subscription that's still in effect.
         FC_ASSERT( d.head_block_time() <= HARDFORK_613_TIME );
         FC_ASSERT(a.membership_expiration_date - d.head_block_time() < fc::days(3650),
                   "May not extend annual membership more than a decade into the future.");
         a.membership_expiration_date += fc::days(365);
      } else {
         // Upgrade from basic account.
         FC_ASSERT( d.head_block_time() <= HARDFORK_613_TIME );
         a.statistics(d).process_fees(a, d);
         assert(a.is_basic_account(d.head_block_time()));
         //a.referrer = a.get_id();
         a.membership_expiration_date = d.head_block_time() + fc::days(365);
      }
   });

   return {};
} FC_RETHROW_EXCEPTIONS( error, "Unable to upgrade account '${a}'", ("a",o.account_to_upgrade(db()).name) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_restrict_evaluator::do_evaluate(const account_restrict_operation& o)
{ try {
    database& d = db();

    const auto& idx = d.get_index_type<restricted_account_index>().indices().get<by_acc_id>();
    auto itr = idx.find(o.target);

    FC_ASSERT( ((o.action & o.restore) && (itr != idx.end())) || (o.action & ~o.restore) );

    const auto& acc_idx = d.get_index_type<account_index>().indices().get<by_id>();
    auto acc_itr = acc_idx.find(o.target);
    FC_ASSERT(acc_itr != acc_idx.end());

    account_ptr = &d.get(o.target);

    if (itr != idx.end()) {
       restricted_account_obj_ptr = &*itr;
    }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type account_restrict_evaluator::do_apply(const account_restrict_operation& o)
{ try {
   database& d = db();

   d.modify<account_object>(*account_ptr, [&](account_object& acc) {
      acc.current_restriction = static_cast<account_restrict_operation::account_action>(o.action);
   } );

   if (o.action & o.restore) {
      d.remove(*restricted_account_obj_ptr);
   }
   else
   {
      if (restricted_account_obj_ptr == nullptr)
      {
         const auto& new_restr_object =
         d.create<restricted_account_object>( [&]( restricted_account_object& rao)
         {
            rao.account = o.target;
            rao.restriction_type = o.action;
         } );
         return new_restr_object.id;
      }

      d.modify<restricted_account_object>( *restricted_account_obj_ptr,[&](restricted_account_object& rao) {
         rao.restriction_type = o.action;
      } );
   }

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_allow_registrar_evaluator::do_evaluate(const account_allow_registrar_operation& o)
{ try {
   database& d = db();

   auto& p = d.get_account_properties();

   auto itr = p.accounts_properties.find(o.target); 
   FC_ASSERT((itr != p.accounts_properties.end()) || !(o.action & o.disallow) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type account_allow_registrar_evaluator::do_apply(const account_allow_registrar_operation& o)
{ try {
   database& d = db();
   const account_properties_object& prop = d.get_account_properties();
   d.modify(prop,[&]( account_properties_object& obj)
   {
      auto itr = obj.accounts_properties.find(o.target);
      if (itr != obj.accounts_properties.end())
      {
         itr->second.can_be_registrar = o.action & o.allow;
      } else {
         account_properties_object::account_properties po;
         po.can_be_registrar = o.action & o.allow;
         obj.accounts_properties.insert(std::make_pair(o.target, po));
      }
   } );

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result set_online_time_evaluator::do_evaluate(const set_online_time_operation& o)
{ try {
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result set_online_time_evaluator::do_apply(const set_online_time_operation& o)
{ try {
   database& d = db();

   d.modify(d.get(accounts_online_id_type()), [&](accounts_online_object& obj) {
      obj.online_info = o.online_info;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result set_verification_is_required_evaluator::do_evaluate(const set_verification_is_required_operation& o)
{ try {
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result set_verification_is_required_evaluator::do_apply(const set_verification_is_required_operation& o)
{ try {
   database& d = db();

   d.modify(o.target(d), [&](account_object& obj) {
      obj.verification_is_required = o.verification_is_required;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result allow_create_addresses_evaluator::do_evaluate(const allow_create_addresses_operation& o)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(o.account_id);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", o.account_id));

   account_ptr = &*itr;

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result allow_create_addresses_evaluator::do_apply(const allow_create_addresses_operation& o)
{ try {
   database& d = db();

   if (account_ptr)
   {
      d.modify<account_object>(*account_ptr, [&](account_object& acc) {
         acc.can_create_addresses = o.allow;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result set_burning_mode_evaluator::do_evaluate(const set_burning_mode_operation& o)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(o.account_id);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", o.account_id));

   account_ptr = &*itr;

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result set_burning_mode_evaluator::do_apply(const set_burning_mode_operation& o)
{ try {
   database& d = db();

   if (account_ptr)
   {
      d.modify<account_object>(*account_ptr, [&](account_object& acc) {
         acc.burning_mode_enabled = o.enabled;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result assets_update_fee_payer_evaluator::do_evaluate(const assets_update_fee_payer_operation& o)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<asset_index>().indices().get<by_id>();
   for (const asset_id_type& item: o.assets_to_update)
   {
      auto asset_iter = idx.find(item);
      FC_ASSERT(asset_iter != idx.end(), "Asset with ID ${a} not exists!", ("a", item));
   }

   auto asset_paying_iter = idx.find(o.fee_payer_asset);
   FC_ASSERT(asset_paying_iter != idx.end(), "Payer asset ${a} not exists!", ("a", o.fee_payer_asset));

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result assets_update_fee_payer_evaluator::do_apply(const assets_update_fee_payer_operation& o)
{ try {
   database& d = db();

   for (const asset_id_type& item: o.assets_to_update)
   {
      d.modify(item(d), [&](asset_object& a) {
         a.params.fee_paying_asset = o.fee_payer_asset;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result asset_update_exchange_rate_evaluator::do_evaluate(const asset_update_exchange_rate_operation& op)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<asset_index>().indices().get<by_id>();

   auto asset_paying_iter = idx.find(op.asset_to_update);
   FC_ASSERT(asset_paying_iter != idx.end(), "Paying asset ${a} not exists!", ("a", op.asset_to_update));

   asset_ptr = &(*asset_paying_iter);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result asset_update_exchange_rate_evaluator::do_apply(const asset_update_exchange_rate_operation& op)
{ try {
   database& d = db();

   if (asset_ptr)
   {
      d.modify<asset_object>(*asset_ptr, [&](asset_object& o) {
         o.options.core_exchange_rate = op.core_exchange_rate;
      } );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result set_market_evaluator::do_evaluate(const set_market_operation& op)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(op.to_account);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", op.to_account));

   account_ptr = &*itr;

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result set_market_evaluator::do_apply(const set_market_operation& op)
{ try {
   database& d = db();

   if (account_ptr)
   {
      d.modify<account_object>(*account_ptr, [&](account_object& acc) {
         acc.is_market = op.enabled;
      });
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result create_market_address_evaluator::do_evaluate(const create_market_address_operation& op)
{ try {
   database& d = db();

   settings_ptr = &(const chain::settings_object&)(d.get(settings_id_type(0)));
   FC_ASSERT(settings_ptr, "Settings not found!");

   const asset_object& asset_type = EDC_ASSET(d);
   asset_dyn_data_ptr = &asset_type.dynamic_asset_data_id(d);

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(op.market_account_id);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", op.market_account_id));
   FC_ASSERT(itr->is_market, "Account with ID ${id} is not a market!", ("a", op.market_account_id));

   if (d.head_block_time() > HARDFORK_627_TIME)
   {
      if (d.head_block_time() > HARDFORK_633_TIME)
      {
         FC_ASSERT(d.get_balance(op.market_account_id, EDC_ASSET).amount >= settings_ptr->create_market_address_fee_edc
                   , "Insufficient Balance: ${balance}, fee ${fee}"
                   , ("balance", d.to_pretty_string(d.get_balance(op.market_account_id, EDC_ASSET)))
                     ("fee", d.to_pretty_string(settings_ptr->create_market_address_fee_edc)));
      }
      else
      {
         FC_ASSERT(d.get_balance(op.market_account_id, EDC_ASSET).amount > settings_ptr->create_market_address_fee_edc
                  , "Insufficient Balance: ${balance}, fee ${fee}"
                  , ("balance", d.to_pretty_string(d.get_balance(op.market_account_id, EDC_ASSET).amount))
                    ("fee", d.to_pretty_string(settings_ptr->create_market_address_fee_edc)));
      }
   }

   if (d.get_index_type<market_address_index>().indices().size() > 0)
   {
      auto idx_addr = --d.get_index_type<market_address_index>().indices().get<by_id>().end();
      const market_address_object& last_address = *idx_addr;

      bool time_is_valid = (last_address.block_num != d.head_block_num())
                           || ((last_address.block_num == d.head_block_num()) && (last_address.transaction_num != d.current_transaction_num()));

      FC_ASSERT( time_is_valid, "Wrong time for address generating");
   }

   if (op.extensions.size() > 0)
   {
      uint32_t count = op.extensions.begin()->get<uint32_t>();
      FC_ASSERT( ((count > 0) && (count <= 100)), "Wrong count of addresses (${c}), limit 1-100", ("c", count));

      addresses = d.get_address_batch(count);
   }
   else {
      addresses = { d.get_address() };
   }

   FC_ASSERT(addresses.size() > 0, "Generating addresses error");

   for (const address& addr: addresses)
   {
      auto addr_itr = d.get_index_type<market_address_index>().indices().get<by_address>().find(addr);
      FC_ASSERT(addr_itr == d.get_index_type<market_address_index>().indices().get<by_address>().end()
                , "The same address ('${a}') is already exists!", ("a", addr));
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

operation_result create_market_address_evaluator::do_apply(const create_market_address_operation& op)
{ try {

   database& d = db();

   market_address result;
   market_addresses result2;

   for (const address& addr: addresses)
   {
      auto next_id = d.get_index_type<market_address_index>().get_next_id();

      const market_address_object& obj = d.create<market_address_object>([&](market_address_object& obj)
      {
         obj.market_account_id = op.market_account_id;
         obj.addr = addr;
         obj.create_datetime = d.head_block_time();
         obj.block_num = d.head_block_num();
         obj.transaction_num = d.current_transaction_num();
      });

      FC_ASSERT(obj.id == next_id);

      if ( (d.head_block_time() > HARDFORK_632_TIME) && (d.head_block_time() < HARDFORK_633_TIME) )
      {
         /**
          * DEPRECATED: not stable, because each witness can generate
          * different addresses (blocknum can be different for each of them)
          */
         result2.addresses.insert(std::make_pair(next_id, std::string(addr)));
      }
      else if (d.head_block_time() <= HARDFORK_632_TIME) // <--- before hardfork that must be always only one address
      {
         result.id = next_id;
         result.addr = std::string(addr);
      }
   }

   // fee
   if ((d.head_block_time() > HARDFORK_627_TIME) && (settings_ptr->create_market_address_fee_edc > 0))
   {
      d.adjust_balance(op.market_account_id, -asset(settings_ptr->create_market_address_fee_edc, EDC_ASSET));

      // current supply
      d.modify(*asset_dyn_data_ptr, [&](asset_dynamic_data_object& data)
      {
         data.current_supply -= settings_ptr->create_market_address_fee_edc;
         data.fee_burnt += settings_ptr->create_market_address_fee_edc;
      });
   }

   if (d.head_block_time() >= HARDFORK_633_TIME) {
      return void_result();
   }
   else if (d.head_block_time() > HARDFORK_632_TIME) {
      return result2;
   }
   else {
      return result;
   }

} FC_CAPTURE_AND_RETHROW( (op) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result account_limit_daily_volume_evaluator::do_evaluate(const account_edc_limit_daily_volume_operation& op)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(op.account_id);

   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("a", op.account_id));

   account_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result account_limit_daily_volume_evaluator::do_apply(const account_edc_limit_daily_volume_operation& op)
{ try {

   database& d = db();

   if (account_ptr)
   {
      d.modify<account_object>(*account_ptr, [&](account_object& acc)
      {
         acc.edc_limit_transfers_enabled = op.limit_transfers_enabled;

         if (op.extensions.size() > 0)
         {
            const protocol::limit_daily_ext_info& ext = op.extensions.begin()->get<protocol::limit_daily_ext_info>();

            acc.edc_transfers_max_amount  = ext.transfers_max_amount;
            acc.edc_limit_cheques_enabled = ext.limit_cheques_enabled;
            acc.edc_cheques_max_amount    = ext.cheques_max_amount;
         }
      });
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result update_accounts_referrer_evaluator::do_evaluate(const update_accounts_referrer_operation& op)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();

   auto itr_ref = idx.find(op.new_referrer);
   FC_ASSERT(itr_ref != idx.end(), "Referrer-account with ID ${id} not exists!", ("id", op.new_referrer));
   FC_ASSERT(op.accounts.size() > 0, "Accounts count must be > 0. Current count: ${n}", ("n", op.accounts.size()));

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_accounts_referrer_evaluator::do_apply(const update_accounts_referrer_operation& op)
{ try {

   database& d = db();
   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();

   for (const account_id_type& acc_id: op.accounts)
   {
      auto itr = idx.find(acc_id);
      if (itr != idx.end())
      {
         d.modify<account_object>(*itr, [&](account_object& obj) {
           obj.referrer = op.new_referrer;
         });
      }
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

//////////////////////////////////////////////////////////////////////////////////////

void_result enable_account_referral_payments_evaluator::do_evaluate(const enable_account_referral_payments_operation& op)
{ try {
   database& d = db();

   const auto& idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto itr = idx.find(op.account_id);
   FC_ASSERT(itr != idx.end(), "Account with ID ${id} not exists!", ("id", op.account_id));

   account_ptr = &(*itr);

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result enable_account_referral_payments_evaluator::do_apply(const enable_account_referral_payments_operation& op)
{ try {

   database& d = db();

   if (account_ptr)
   {
      d.modify<account_object>(*account_ptr, [&](account_object& obj) {
         obj.referral_payments_enabled = op.enabled;
      });
   }

   return void_result();

} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
