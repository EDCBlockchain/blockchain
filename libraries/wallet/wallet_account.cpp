#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include "wallet_api_impl.hpp"
#include <graphene/wallet/wallet.hpp>
#include <graphene/protocol/pts_address.hpp>

namespace graphene { namespace wallet { namespace detail {

   account_object wallet_api_impl::get_account(account_id_type id) const
   {
      auto rec = _remote_db->get_accounts({id}).front();
      FC_ASSERT(rec);
      return *rec;
   }

   account_object wallet_api_impl::get_account(const string& account_name_or_id) const
   {
      FC_ASSERT( account_name_or_id.size() > 0 );

      if (auto id = maybe_id<account_id_type>(account_name_or_id))
      {
         // It's an ID
         return get_account(*id);
      }
      else
      {
         // It's a name
         auto rec = _remote_db->lookup_account_names({account_name_or_id}).front();
         if (!rec) {
            elog("account_name_or_id is not exists: ${a} (file: ${f})", ("a", account_name_or_id)("f", __FILE__));
         }

         FC_ASSERT( rec && rec->name == account_name_or_id );
         return *rec;
      }
   }

   account_id_type wallet_api_impl::get_account_id(const string& account_name) const {
      return get_account(account_name).get_id();
   }

   vector<signed_transaction> wallet_api_impl::import_balance( string name_or_id, const vector<string>& wif_keys,                                                           bool broadcast )
   { try {
   FC_ASSERT(!is_locked());
   const dynamic_global_property_object& dpo = _remote_db->get_dynamic_global_properties();
   account_object claimer = get_account( name_or_id );
   uint32_t max_ops_per_tx = 30;

   map< address, private_key_type > keys;  // local index of address -> private key
   vector< address > addrs;
   bool has_wildcard = false;
   addrs.reserve( wif_keys.size() );
   for( const string& wif_key : wif_keys )
   {
      if( wif_key == "*" )
      {
         if( has_wildcard )
            continue;
         for( const public_key_type& pub : _wallet.extra_keys[ claimer.id ] )
         {
            addrs.push_back( pub );
            auto it = _keys.find( pub );
            if( it != _keys.end() )
            {
               fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
               FC_ASSERT( privkey );
               keys[ addrs.back() ] = *privkey;
            }
            else
            {
               wlog( "Somehow _keys has no private key for extra_keys public key ${k}", ("k", pub) );
            }
         }
         has_wildcard = true;
      }
      else
      {
         fc::optional< private_key_type > key = wif_to_key( wif_key );
         FC_ASSERT( key.valid(), "Invalid private key" );
         fc::ecc::public_key pk = key->get_public_key();
         addrs.push_back( pk );
         keys[addrs.back()] = *key;
         // see chain/balance_evaluator.cpp
         addrs.push_back( pts_address( pk, false, 56 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, true, 56 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, false, 0 ) );
         keys[addrs.back()] = *key;
         addrs.push_back( pts_address( pk, true, 0 ) );
         keys[addrs.back()] = *key;
      }
   }

   vector< balance_object > balances = _remote_db->get_balance_objects( addrs );

   wdump((balances));
   addrs.clear();

   set<asset_id_type> bal_types;
   for( auto b : balances ) bal_types.insert( b.balance.asset_id );

   struct claim_tx
   {
      vector< balance_claim_operation > ops;
      set< address > addrs;
   };
   vector< claim_tx > claim_txs;

   for( const asset_id_type& a : bal_types )
   {
      balance_claim_operation op;
      op.deposit_to_account = claimer.id;
      for( const balance_object& b : balances )
      {
         if( b.balance.asset_id == a )
         {
            op.total_claimed = b.available( dpo.time );
            if( op.total_claimed.amount == 0 )
               continue;
            op.balance_to_claim = b.id;
            op.balance_owner_key = keys[b.owner].get_public_key();
            if( (claim_txs.empty()) || (claim_txs.back().ops.size() >= max_ops_per_tx) )
               claim_txs.emplace_back();
            claim_txs.back().ops.push_back(op);
            claim_txs.back().addrs.insert(b.owner);
         }
      }
   }

   vector< signed_transaction > result;

   for( const claim_tx& ctx : claim_txs )
   {
      signed_transaction tx;
      tx.operations.reserve( ctx.ops.size() );
      for( const balance_claim_operation& op : ctx.ops )
         tx.operations.emplace_back( op );
      asset_object core_asset = get_asset("CORE");
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), core_asset.options.core_exchange_rate );
      tx.validate();
      signed_transaction signed_tx = sign_transaction( tx, false );
      for( const address& addr : ctx.addrs )
         signed_tx.sign( keys[addr], _chain_id );
      // if the key for a balance object was the same as a key for the account we're importing it into,
      // we may end up with duplicate signatures, so remove those
      boost::erase(signed_tx.signatures, boost::unique<boost::return_found_end>(boost::sort(signed_tx.signatures)));
      result.push_back( signed_tx );
      if( broadcast )
         _remote_net_broadcast->broadcast_transaction(signed_tx);
   }

   return result;
   } FC_CAPTURE_AND_RETHROW( (name_or_id) ) }

   signed_transaction wallet_api_impl::register_account(string name,
      public_key_type owner,
      public_key_type active,
      string  registrar_account,
      string  referrer_account,
      uint32_t referrer_percent,
      bool broadcast)
   { try {
      FC_ASSERT( !self.is_locked() );
      FC_ASSERT( is_valid_name(name) );
      account_create_operation account_create_op;

      // #449 referrer_percent is on 0-100 scale, if user has larger
      // number it means their script is using GRAPHENE_100_PERCENT scale
      // instead of 0-100 scale.
      FC_ASSERT( referrer_percent <= 100 );
      // TODO:  process when pay_from_account is ID
      account_object registrar_account_object =
            this->get_account( registrar_account );
//      FC_ASSERT( registrar_account_object.is_lifetime_member() );

      account_id_type registrar_account_id = registrar_account_object.id;

      account_object referrer_account_object =
            this->get_account( referrer_account );
      account_create_op.referrer = referrer_account_object.id;
      account_create_op.referrer_percent = uint16_t( referrer_percent * GRAPHENE_1_PERCENT );

      account_create_op.registrar = registrar_account_id;
      account_create_op.name = name;
      account_create_op.owner = authority(1, owner, 1);
      account_create_op.active = authority(1, active, 1);
      account_create_op.options.memo_key = active;

      signed_transaction tx;

      tx.operations.push_back( account_create_op );

      auto current_fees = _remote_db->get_global_properties().parameters.get_current_fees();
      set_operation_fees( tx, current_fees );

      vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

      auto dyn_props = get_dynamic_global_properties();
      tx.set_reference_block( dyn_props.head_block_id );
      tx.set_expiration( dyn_props.time + fc::seconds(30) );
      tx.validate();

      for( public_key_type& key : paying_keys )
      {
         auto it = _keys.find(key);
         if( it != _keys.end() )
         {
            fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
            if( !privkey.valid() )
            {
               FC_ASSERT( false, "Malformed private key in _keys" );
            }
            tx.sign( *privkey, _chain_id );
         }
      }

      if( broadcast )
         _remote_net_broadcast->broadcast_transaction( tx );
      return tx;
   } FC_CAPTURE_AND_RETHROW( (name)(owner)(active)(registrar_account)(referrer_account)(referrer_percent)(broadcast) ) }

   signed_transaction wallet_api_impl::upgrade_account(string name, bool broadcast)
   { try {
      FC_ASSERT( !self.is_locked() );
      account_object account_obj = get_account(name);
      FC_ASSERT( !account_obj.is_lifetime_member() );

      signed_transaction tx;
      account_upgrade_operation op;
      op.account_to_upgrade = account_obj.get_id();
      op.upgrade_to_lifetime_member = true;
      tx.operations = {op};
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (name) ) }
   
   signed_transaction wallet_api_impl::create_account_with_private_key(fc::ecc::private_key owner_privkey,
      string account_name,
      string registrar_account,
      string referrer_account,
      bool broadcast,
      bool save_wallet)
   { try {
         int active_key_index = find_first_unused_derived_key_index(owner_privkey);
         fc::ecc::private_key active_privkey = derive_private_key( key_to_wif(owner_privkey), active_key_index);

         int memo_key_index = find_first_unused_derived_key_index(active_privkey);
         fc::ecc::private_key memo_privkey = derive_private_key( key_to_wif(active_privkey), memo_key_index);

         graphene::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
         graphene::chain::public_key_type active_pubkey = active_privkey.get_public_key();
         graphene::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

         account_create_operation account_create_op;

         // TODO:  process when pay_from_account is ID

         account_object registrar_account_object = get_account( registrar_account );

         account_id_type registrar_account_id = registrar_account_object.id;

         account_object referrer_account_object = get_account( referrer_account );
         account_create_op.referrer = referrer_account_object.id;
         account_create_op.referrer_percent = referrer_account_object.referrer_rewards_percentage;

         account_create_op.registrar = registrar_account_id;
         account_create_op.name = account_name;
         account_create_op.owner = authority(1, owner_pubkey, 1);
         account_create_op.active = authority(1, active_pubkey, 1);
         account_create_op.options.memo_key = memo_pubkey;

         // current_fee_schedule()
         // find_account(pay_from_account)

         // account_create_op.fee = account_create_op.calculate_fee(db.current_fee_schedule());

         signed_transaction tx;

         tx.operations.push_back( account_create_op );

         set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());

         vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

         auto dyn_props = get_dynamic_global_properties();
         tx.set_reference_block( dyn_props.head_block_id );
         tx.set_expiration( dyn_props.time + fc::seconds(30) );
         tx.validate();

         for( public_key_type& key : paying_keys )
         {
            auto it = _keys.find(key);
            if( it != _keys.end() )
            {
               fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
               FC_ASSERT( privkey.valid(), "Malformed private key in _keys" );
               tx.sign( *privkey, _chain_id );
            }
         }

         // we do not insert owner_privkey here because
         //    it is intended to only be used for key recovery
         _wallet.pending_account_registrations[account_name].push_back(key_to_wif( active_privkey ));
         _wallet.pending_account_registrations[account_name].push_back(key_to_wif( memo_privkey ));
         if( save_wallet )
            save_wallet_file();
         if( broadcast )
            _remote_net_broadcast->broadcast_transaction( tx );
         return tx;
   } FC_CAPTURE_AND_RETHROW( (account_name)(registrar_account)(referrer_account)(broadcast) ) }
   
   signed_transaction wallet_api_impl::create_account_with_brain_key(string brain_key,
      string account_name,
      string registrar_account,
      string referrer_account,
      bool broadcast,
      bool save_wallet)
   { try {
      FC_ASSERT( !self.is_locked() );
      string normalized_brain_key = normalize_brain_key( brain_key );
      // TODO:  scan blockchain for accounts that exist with same brain key
      fc::ecc::private_key owner_privkey = derive_private_key( normalized_brain_key, 0 );
      return create_account_with_private_key(owner_privkey, account_name, registrar_account, referrer_account, broadcast, save_wallet);
   } FC_CAPTURE_AND_RETHROW( (account_name)(registrar_account)(referrer_account) ) }
   
   signed_transaction wallet_api_impl::whitelist_account(string authorizing_account,
      string account_to_list,
      account_whitelist_operation::account_listing new_listing_status,
      bool broadcast)
   { try {
      account_whitelist_operation whitelist_op;
      whitelist_op.authorizing_account = get_account_id(authorizing_account);
      whitelist_op.account_to_list = get_account_id(account_to_list);
      whitelist_op.new_listing = new_listing_status;

      signed_transaction tx;
      tx.operations.push_back( whitelist_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (authorizing_account)(account_to_list)(new_listing_status)(broadcast) ) }
   
   vector<vesting_balance_object_with_info> wallet_api_impl::get_vesting_balances(string account_name)
   { try {
      fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>( account_name );
      std::vector<vesting_balance_object_with_info> result;
      fc::time_point_sec now = _remote_db->get_dynamic_global_properties().time;

      if( vbid )
      {
         result.emplace_back( get_object(*vbid), now );
         return result;
      }

      // try casting to avoid a round-trip if we were given an account ID
      fc::optional<account_id_type> acct_id = maybe_id<account_id_type>( account_name );
      if( !acct_id )
         acct_id = get_account( account_name ).id;

      vector< vesting_balance_object > vbos = _remote_db->get_vesting_balances( *acct_id );
      if( vbos.size() == 0 )
         return result;

      for( const vesting_balance_object& vbo : vbos )
         result.emplace_back( vbo, now );

      return result;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }
   
   void wallet_api_impl::claim_registered_account(const account_object& account)
   {
      auto it = _wallet.pending_account_registrations.find(account.name);
      FC_ASSERT( it != _wallet.pending_account_registrations.end() );
      for (const std::string& wif_key: it->second)
      {
         if (!import_key(account.name, wif_key))
         {
            // somebody else beat our pending registration, there is
            //    nothing we can do except log it and move on
            elog("account ${name} registered by someone else first!", ("name", account.name));
            // might as well remove it from pending regs,
            //    because there is now no way this registration
            //    can become valid (even in the extremely rare
            //    possibility of migrating to a fork where the
            //    name is available, the user can always
            //    manually re-register)
         }
      }
      _wallet.pending_account_registrations.erase(it);
   }

   void wallet_api_impl::claim_registered_witness(const std::string& witness_name)
   {
      auto iter = _wallet.pending_witness_registrations.find(witness_name);
      FC_ASSERT(iter != _wallet.pending_witness_registrations.end());
      std::string wif_key = iter->second;

      // get the list key id this key is registered with in the chain
      fc::optional<fc::ecc::private_key> witness_private_key = wif_to_key(wif_key);
      FC_ASSERT(witness_private_key);

      auto pub_key = witness_private_key->get_public_key();
      _keys[pub_key] = wif_key;
      _wallet.pending_witness_registrations.erase(iter);
   }
   
   signed_transaction wallet_api_impl::propose_account_restriction(const string& initiator,
      const string& target, account_restrict_operation::account_action action,
      fc::time_point_sec expiration_time, bool broadcast)
   { try {
         account_restrict_operation rest_op;
         rest_op.target = get_account_id(target);
         rest_op.action = action;

         proposal_create_operation prop_op;

         prop_op.expiration_time = expiration_time;
         prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
         prop_op.fee_paying_account = get_account_id(initiator);
         prop_op.proposed_ops.emplace_back( rest_op );

         signed_transaction tx;
         tx.operations.push_back(prop_op);
         set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
         tx.validate();

         return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(target)(action)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::propose_account_registrar_permission(const string& initiator,
      const string& target, account_allow_registrar_operation::account_action action,
      fc::time_point_sec expiration_time, bool broadcast)
   { try {

         account_allow_registrar_operation rest_op;
         rest_op.target = get_account_id(target);
         rest_op.action = action;

         proposal_create_operation prop_op;

         prop_op.expiration_time = expiration_time;
         prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
         prop_op.fee_paying_account = get_account_id(initiator);

         prop_op.proposed_ops.emplace_back( rest_op );

         signed_transaction tx;
         tx.operations.push_back(prop_op);
         set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
         tx.validate();

         return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(target)(action)(expiration_time)(broadcast) ) }

   signed_transaction wallet_api_impl::propose_allow_create_asset(const string& initiator,
      const string& target, bool allow, fc::time_point_sec expiration_time, bool broadcast)
   { try {
      allow_create_asset_operation op;
      op.to_account = get_account_id(target);
      op.value = allow;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back( op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(target)(allow)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::propose_allow_create_addresses(const string& initiator,
      const string& target, bool allow, fc::time_point_sec expiration_time, bool broadcast)
   { try {
      allow_create_addresses_operation op;
      op.account_id = get_account_id(target);
      op.allow = allow;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back( op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(target)(allow)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::propose_update_account_authorities(
      const string& initiator
      , const string& target
      , const public_key_type& owner_key
      , const public_key_type& active_key
      , const public_key_type& memo_key
      , fc::time_point_sec expiration_time
      , bool broadcast)
   { try {
      account_update_authorities_operation op;
      op.account = get_account_id(target);
      op.owner = authority(1, owner_key, 1);
      op.active = authority(1, active_key, 1);
      op.memo_key = memo_key;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back( op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(target)(owner_key)(active_key)(memo_key)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::set_market(const std::string& account, bool enabled)
   { try {

      account_object to_account = get_account(account);

      set_market_operation op;
      op.to_account = to_account.get_id();
      op.enabled = enabled;

      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      bool broadcast = true;

      return sign_transaction( tx, broadcast);

   } FC_CAPTURE_AND_RETHROW( (account)(enabled) ) }
   
   account_object wallet_api_impl::get_market_by_address(const std::string& addr) const
   { try {

      fc::optional<account_id_type> market_account_id = _remote_db->get_market_reference(address(addr));
      FC_ASSERT(market_account_id, "There is no market associated with this address: ${a}", ("a", addr));
      const vector<fc::optional<account_object>>& v = _remote_db->get_accounts({*market_account_id});
      FC_ASSERT(v.size() == 1);
      return *v[0];

   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   flat_set<vote_id_type> wallet_api_impl::get_account_votes(const std::string& name_or_id) const
   { try {

     account_object acc = get_account( name_or_id );
     return acc.options.votes;

   } FC_CAPTURE_AND_RETHROW( (name_or_id) ) }

   fc::optional<restricted_account_object>
   wallet_api_impl::get_restricted_account(const std::string& name_or_id) const
   {  try {

      account_object acc = get_account( name_or_id );
      return _remote_db->get_restricted_account(acc.get_id());

   } FC_CAPTURE_AND_RETHROW( (name_or_id) ) }
   
   std::pair<unsigned, vector<address>>
   wallet_api_impl::get_account_addresses(const string& name_or_id, unsigned from, unsigned limit) {
      return _remote_db->get_account_addresses(name_or_id, from, limit);
   }

   std::vector<blind_transfer2_object>
   wallet_api_impl::get_account_blind_transfers2(const string& name_or_id, unsigned from, unsigned limit)
   {
      std::vector<blind_transfer2_object> result;

      if (_remote_secure)
      {
         const account_object& acc = get_account(name_or_id);
         return (*_remote_secure)->get_account_blind_transfers2(acc.get_id(), from, limit);
      }

      return result;
   }

   std::vector<cheque_object>
   wallet_api_impl::get_account_cheques(const string& name_or_id, unsigned from, unsigned limit)
   {
      std::vector<cheque_object> result;

      if (_remote_secure)
      {
         const account_object& acc = get_account(name_or_id);
         return (*_remote_secure)->get_account_cheques(acc.get_id(), from, limit);
      }

      return result;
   }
   
   signed_transaction wallet_api_impl::set_verification_is_required( account_id_type target, bool verification_is_required )
   { try {
      bool broadcast = true;
      fc::optional<asset_object> fee_asset_obj = get_asset("CORE");
      FC_ASSERT(fee_asset_obj, "Could not find asset matching ${asset}", ("asset", "CORE"));

      set_verification_is_required_operation set_op;
      set_op.target = target;
      set_op.verification_is_required = verification_is_required;
      signed_transaction tx;
      tx.operations.push_back(set_op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), fee_asset_obj->options.core_exchange_rate );
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (target)(verification_is_required) ) }
   
   signed_transaction wallet_api_impl::set_account_limit_daily_volume(const std::string& name_or_id, bool enabled)
   { try {
      bool broadcast = true;
      fc::optional<asset_object> fee_asset_obj = get_asset("CORE");
      FC_ASSERT(fee_asset_obj, "Could not find asset matching ${asset}", ("asset", "CORE"));

      const account_object& acc = get_account(name_or_id);

      account_edc_limit_daily_volume_operation op;
      op.account_id = acc.get_id();
      op.limit_transfers_enabled = enabled;
      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), fee_asset_obj->options.core_exchange_rate );
      // void set_operation_fees(signed_transaction& tx, const fee_schedule& s, price& ex_rate)
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (name_or_id)(enabled) ) }

   fc::optional<witness_object> wallet_api_impl::get_witness_by_account(const std::string& name_or_id)
   { try {
      const account_object& acc = get_account(name_or_id);
      return _remote_db->get_witness_by_account(acc.get_id());
   } FC_CAPTURE_AND_RETHROW( (name_or_id) ) }

   signed_transaction wallet_api_impl::set_witness_exception(
      const std::vector<account_id_type>& exc_accounts_blocks
      , const std::vector<account_id_type>& exc_accounts_fees
      , bool exception_enabled)
   { try {
      bool broadcast = true;
      fc::optional<asset_object> fee_asset_obj = get_asset("CORE");
      FC_ASSERT(fee_asset_obj, "Could not find asset matching ${asset}", ("asset", "CORE"));

      set_witness_exception_operation op;
      op.exc_accounts_blocks = exc_accounts_blocks;
      op.exc_accounts_fees   = exc_accounts_fees;
      op.exception_enabled   = exception_enabled;
      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), fee_asset_obj->options.core_exchange_rate );
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (exc_accounts_blocks)(exc_accounts_fees)(exception_enabled) ) }

   signed_transaction wallet_api_impl::update_accounts_referrer(const std::vector<account_id_type>& accounts, const string& new_referrer)
   {
      bool broadcast = true;

      FC_ASSERT(accounts.size() > 0, "account list to update is empty");
      const account_object& referrer = get_account(new_referrer);

      update_accounts_referrer_operation op;
      op.accounts = accounts;
      op.new_referrer = referrer.get_id();
      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }
   
   signed_transaction wallet_api_impl::enable_account_referral_payments(const string& account_id_or_name, bool enabled)
   {
      bool broadcast = true;

      const account_object& acc = get_account(account_id_or_name);

      enable_account_referral_payments_operation op;
      op.account_id = acc.get_id();
      op.enabled = enabled;
      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

}}} // namespace graphene::wallet::detail