#include <fc/crypto/aes.hpp>

#include "wallet_api_impl.hpp"
#include <graphene/wallet/wallet.hpp>

/***
 * These methods handle signing and keys
 */

namespace graphene { namespace wallet { namespace detail {

   string address_to_shorthash( const address& addr )
   {
      uint32_t x = addr.addr._hash[0].value();
      static const char hd[] = "0123456789abcdef";
      string result;

      result += hd[(x >> 0x1c) & 0x0f];
      result += hd[(x >> 0x18) & 0x0f];
      result += hd[(x >> 0x14) & 0x0f];
      result += hd[(x >> 0x10) & 0x0f];
      result += hd[(x >> 0x0c) & 0x0f];
      result += hd[(x >> 0x08) & 0x0f];
      result += hd[(x >> 0x04) & 0x0f];
      result += hd[(x        ) & 0x0f];

      return result;
   }

   fc::ecc::private_key derive_private_key( const std::string& prefix_string, int sequence_number )
   {
      std::string sequence_string = std::to_string(sequence_number);
      fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
      fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
      return derived_key;
   }

   string normalize_brain_key( string s )
   {
      size_t i = 0, n = s.length();
      std::string result;
      char c;
      result.reserve( n );

      bool preceded_by_whitespace = false;
      bool non_empty = false;
      while( i < n )
      {
         c = s[i++];
         switch( c )
         {
            case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
            preceded_by_whitespace = true;
            continue;

         case 'a': c = 'A'; break;
         case 'b': c = 'B'; break;
         case 'c': c = 'C'; break;
         case 'd': c = 'D'; break;
         case 'e': c = 'E'; break;
         case 'f': c = 'F'; break;
         case 'g': c = 'G'; break;
         case 'h': c = 'H'; break;
         case 'i': c = 'I'; break;
         case 'j': c = 'J'; break;
         case 'k': c = 'K'; break;
         case 'l': c = 'L'; break;
         case 'm': c = 'M'; break;
         case 'n': c = 'N'; break;
         case 'o': c = 'O'; break;
         case 'p': c = 'P'; break;
         case 'q': c = 'Q'; break;
         case 'r': c = 'R'; break;
         case 's': c = 'S'; break;
         case 't': c = 'T'; break;
         case 'u': c = 'U'; break;
         case 'v': c = 'V'; break;
         case 'w': c = 'W'; break;
         case 'x': c = 'X'; break;
         case 'y': c = 'Y'; break;
         case 'z': c = 'Z'; break;

         default:
            break;
         }
         if (preceded_by_whitespace && non_empty) {
            result.push_back(' ');
         }

         result.push_back(c);
         preceded_by_whitespace = false;
         non_empty = true;
      }
      return result;
   }

   void wallet_api_impl::encrypt_keys()
   {
      if( !is_locked() )
      {
         plain_keys data;
         data.keys = _keys;
         data.checksum = _checksum;
         auto plain_txt = fc::raw::pack(data);
         _wallet.cipher_keys = fc::aes_encrypt( data.checksum, plain_txt );
      }
   }

   fc::ecc::private_key wallet_api_impl::get_private_key(const public_key_type& id) const
   {
      auto it = _keys.find(id);
      FC_ASSERT( it != _keys.end() );

      fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
      FC_ASSERT( privkey );
      return *privkey;
   }

   fc::ecc::private_key wallet_api_impl::get_private_key_for_account(const account_object& account) const
   {
      vector<public_key_type> active_keys = account.active.get_keys();
      if (active_keys.size() != 1) {
         FC_THROW("Expecting a simple authority with one active key");
      }
      return get_private_key(active_keys.front());
   }

   bool wallet_api_impl::import_key(string account_name_or_id, string wif_key)
   {
      fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
      if (!optional_private_key) {
         FC_THROW("Invalid private key");
      }

      graphene::chain::public_key_type wif_pub_key = optional_private_key->get_public_key();

      account_object account = get_account( account_name_or_id );

      // make a list of all current public keys for the named account
      flat_set<public_key_type> all_keys_for_account;
      std::vector<public_key_type> active_keys = account.active.get_keys();
      std::vector<public_key_type> owner_keys = account.owner.get_keys();
      std::copy(active_keys.begin(), active_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
      std::copy(owner_keys.begin(), owner_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
      all_keys_for_account.insert(account.options.memo_key);

      _keys[wif_pub_key] = wif_key;

      _wallet.update_account(account);

      _wallet.extra_keys[account.id].insert(wif_pub_key);

      return all_keys_for_account.find(wif_pub_key) != all_keys_for_account.end();
   }

   signed_transaction wallet_api_impl::sign_transaction(signed_transaction tx, bool broadcast)
   {
      flat_set<account_id_type> req_active_approvals;
      flat_set<account_id_type> req_owner_approvals;
      vector<authority> other_auths;

      tx.get_required_authorities( req_active_approvals, req_owner_approvals, other_auths );

      for( const auto& auth : other_auths )
         for( const auto& a : auth.account_auths )
            req_active_approvals.insert(a.first);

      // std::merge lets us de-duplicate account_id's that occur in both
      //   sets, and dump them into a vector (as required by remote_db api)
      //   at the same time
      vector<account_id_type> v_approving_account_ids;
      std::merge(req_active_approvals.begin(), req_active_approvals.end(),
                 req_owner_approvals.begin() , req_owner_approvals.end(),
                 std::back_inserter(v_approving_account_ids));

      /// TODO: fetch the accounts specified via other_auths as well.

      vector< fc::optional<account_object> > approving_account_objects =
            _remote_db->get_accounts( v_approving_account_ids );

      /// TODO: recursively check one layer deeper in the authority tree for keys

      FC_ASSERT( approving_account_objects.size() == v_approving_account_ids.size() );

      flat_map<account_id_type, account_object*> approving_account_lut;
      size_t i = 0;
      for( fc::optional<account_object>& approving_acct : approving_account_objects )
      {
         if( !approving_acct.valid() )
         {
            wlog( "operation_get_required_auths said approval of non-existing account ${id} was needed",
                  ("id", v_approving_account_ids[i]) );
            i++;
            continue;
         }
         approving_account_lut[ approving_acct->id ] = &(*approving_acct);
         i++;
      }

      flat_set<public_key_type> approving_key_set;
      for( account_id_type& acct_id : req_active_approvals )
      {
         const auto it = approving_account_lut.find( acct_id );
         if( it == approving_account_lut.end() )
            continue;
         const account_object* acct = it->second;
         vector<public_key_type> v_approving_keys = acct->active.get_keys();
         for( const public_key_type& approving_key : v_approving_keys )
            approving_key_set.insert( approving_key );
      }
      for( account_id_type& acct_id : req_owner_approvals )
      {
         const auto it = approving_account_lut.find( acct_id );
         if( it == approving_account_lut.end() )
            continue;
         const account_object* acct = it->second;
         vector<public_key_type> v_approving_keys = acct->owner.get_keys();
         for( const public_key_type& approving_key : v_approving_keys )
            approving_key_set.insert( approving_key );
      }
      for( const authority& a : other_auths )
      {
         for( const auto& k : a.key_auths )
            approving_key_set.insert( k.first );
      }

      auto dyn_props = get_dynamic_global_properties();
      tx.set_reference_block( dyn_props.head_block_id );

      // first, some bookkeeping, expire old items from _recently_generated_transactions
      // since transactions include the head block id, we just need the index for keeping transactions unique
      // when there are multiple transactions in the same block.  choose a time period that should be at
      // least one block long, even in the worst case.  2 minutes ought to be plenty.
      fc::time_point_sec oldest_transaction_ids_to_track(dyn_props.time - fc::minutes(2));
      auto oldest_transaction_record_iter = _recently_generated_transactions.get<timestamp_index>().lower_bound(oldest_transaction_ids_to_track);
      auto begin_iter = _recently_generated_transactions.get<timestamp_index>().begin();
      _recently_generated_transactions.get<timestamp_index>().erase(begin_iter, oldest_transaction_record_iter);

      uint32_t expiration_time_offset = 0;
      for (;;)
      {
         tx.set_expiration( dyn_props.time + fc::seconds(30 + expiration_time_offset) );
         tx.signatures.clear();

         for( public_key_type& key : approving_key_set )
         {
            auto it = _keys.find(key);
            if( it != _keys.end() )
            {
               fc::optional<fc::ecc::private_key> privkey = wif_to_key( it->second );
               FC_ASSERT( privkey.valid(), "Malformed private key in _keys" );
               tx.sign( *privkey, _chain_id );
            }
            /// TODO: if transaction has enough signatures to be "valid" don't add any more,
            /// there are cases where the wallet may have more keys than strictly necessary and
            /// the transaction will be rejected if the transaction validates without requiring
            /// all signatures provided
         }

         graphene::chain::transaction_id_type this_transaction_id = tx.id();
         auto iter = _recently_generated_transactions.find(this_transaction_id);
         if (iter == _recently_generated_transactions.end())
         {
            // we haven't generated this transaction before, the usual case
            recently_generated_transaction_record this_transaction_record;
            this_transaction_record.generation_time = dyn_props.time;
            this_transaction_record.transaction_id = this_transaction_id;
            _recently_generated_transactions.insert(this_transaction_record);
            break;
         }

         // else we've generated a dupe, increment expiration time and re-sign it
         ++expiration_time_offset;
      }

      if (broadcast)
      {
         try {
            _remote_net_broadcast->broadcast_transaction( tx );
         }
         catch (const fc::exception& e)
         {
            elog("Caught exception while broadcasting tx ${id}:  ${e}", ("id", tx.id().str())("e", e.to_detail_string()) );
            throw;
         }
      }

      return tx;
   }
   
   signed_transaction wallet_api_impl::approve_proposal(
      const string& fee_paying_account,
      const string& proposal_id,
      const approval_delta& delta,
      bool broadcast)
   {
      proposal_update_operation update_op;

      update_op.fee_paying_account = get_account(fee_paying_account).id;
      update_op.proposal = fc::variant(proposal_id, 1).as<proposal_id_type>( 1 );
      // make sure the proposal exists
      get_object( update_op.proposal );

      for( const std::string& name : delta.active_approvals_to_add )
         update_op.active_approvals_to_add.insert( get_account( name ).id );
      for( const std::string& name : delta.active_approvals_to_remove )
         update_op.active_approvals_to_remove.insert( get_account( name ).id );
      for( const std::string& name : delta.owner_approvals_to_add )
         update_op.owner_approvals_to_add.insert( get_account( name ).id );
      for( const std::string& name : delta.owner_approvals_to_remove )
         update_op.owner_approvals_to_remove.insert( get_account( name ).id );
      for( const std::string& k : delta.key_approvals_to_add )
         update_op.key_approvals_to_add.insert( public_key_type( k ) );
      for( const std::string& k : delta.key_approvals_to_remove )
         update_op.key_approvals_to_remove.insert( public_key_type( k ) );

      signed_transaction tx;
      tx.operations.push_back(update_op);
      set_operation_fees(tx, get_global_properties().parameters.get_current_fees());
      tx.validate();
      return sign_transaction(tx, broadcast);
   }

}}} // namespace graphene::wallet::detail