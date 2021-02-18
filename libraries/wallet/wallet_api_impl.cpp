#include <chrono>
#include <thread>

#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptors.hpp>

#include <fc/rpc/api_connection.hpp>
#include <fc/popcount.hpp>
#include <fc/git_revision.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/io/fstream.hpp>

#include <graphene/wallet/wallet.hpp>
#include "wallet_api_impl.hpp"
#include <graphene/utilities/git_revision.hpp>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
#endif

// explicit instantiation for later use
namespace fc {
	template class api<graphene::wallet::wallet_api, identity_member_with_optionals>;
}

/****
 * General methods for wallet impl object (ctor, info, about, wallet file, etc.)
 */

namespace graphene { namespace wallet { namespace detail {

    wallet_api_impl::wallet_api_impl( wallet_api& s, const wallet_data& initial_data, fc::api<login_api> rapi )
      : self(s),
        _chain_id(initial_data.chain_id),
        _remote_api(rapi),
        _remote_db(rapi->database()),
        _remote_net_broadcast(rapi->network_broadcast()),
        _remote_hist(rapi->history())
   {
      chain_id_type remote_chain_id = _remote_db->get_chain_id();
      if (remote_chain_id != _chain_id)
      {
         FC_THROW( "Remote server gave us an unexpected chain_id",
            ("remote_chain_id", remote_chain_id)
            ("chain_id", _chain_id) );
      }
      init_prototype_ops();

      _remote_db->set_block_applied_callback([this](const variant& block_id) {
         on_block_applied(block_id);
      } );

      _wallet.chain_id    = _chain_id;
      _wallet.ws_server   = initial_data.ws_server;
      _wallet.ws_user     = initial_data.ws_user;
      _wallet.ws_password = initial_data.ws_password;
   }

   wallet_api_impl::~wallet_api_impl()
   {
      try {
         _remote_db->cancel_all_subscriptions();
      }
      catch (const fc::exception& e)
      {
         // Right now the wallet_api has no way of knowing if the connection to the
         // witness has already disconnected (via the witness node exiting first).
         // If it has exited, cancel_all_subscriptsions() will throw and there's
         // nothing we can do about it.
         // dlog("Caught exception ${e} while canceling database subscriptions", ("e", e));
      }
   }
  
   signed_transaction wallet_api_impl::set_online_time( map<account_id_type, uint16_t> online_info)
   { try {
      bool broadcast = true;
      fc::optional<asset_object> fee_asset_obj = get_asset("CORE");
      FC_ASSERT(fee_asset_obj, "Could not find asset matching ${asset}", ("asset", "CORE"));

      set_online_time_operation op;
      op.online_info = online_info;
      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), fee_asset_obj->options.core_exchange_rate );
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (online_info) ) }

   signed_transaction wallet_api_impl::generate_address(const string& account_id_or_name)
   {
      const account_object& acc = get_account(account_id_or_name);

      FC_ASSERT(acc.can_create_addresses, "Account ${a} can't create addresses (restricted by committee)!", ("a", acc.name));

      add_address_operation op;
      op.to_account = acc.get_id();

      signed_transaction tx;
      tx.operations.push_back(op);
      tx.validate();

      return sign_transaction(tx, true);
   }

   signed_transaction wallet_api_impl::generate_market_address(const string& account_id_or_name, const std::string& notes)
   {
      const account_object& acc = get_account(account_id_or_name);

      FC_ASSERT(acc.is_market, "Account ${a} isn't a market!", ("a", acc.name));

      create_market_address_operation op;
      op.market_account_id = acc.get_id();
      op.notes = notes;

      signed_transaction tx;
      tx.operations.push_back(op);
      tx.validate();

      return sign_transaction(tx, true);
   }

   variant wallet_api_impl::get_history_operation(object_id_type id) const
   {
      fc::variants result = _remote_db->get_objects({id});

      FC_ASSERT(result.size() == 1);
      FC_ASSERT(!is_locked(), "wallet should be unlocked to see memo!");
      FC_ASSERT((id.type() == 11), "ID should be only for 'operation history' object!");

      fc::mutable_variant_object res = result[0].get_object();

      const fc::variant_object& op_root = res["op"].get_array()[1].get_object();
      fc::mutable_variant_object fee    = op_root["fee"].get_object();
      const std::string& from           = op_root["from"].as_string();
      const std::string& to             = op_root["to"].as_string();
      fc::mutable_variant_object amount = op_root["amount"].get_object();

      fc::mutable_variant_object op;
      op.set("fee", fee);
      op.set("from", from);
      op.set("to", to);
      op.set("amount", amount);

      //////////// memo

      if (op_root.contains("memo"))
      {
         chain::memo_data memo_obj = op_root["memo"].as<chain::memo_data>(1);

         try
         {
            FC_ASSERT((_keys.count(memo_obj.to) > 0) || (_keys.count(memo_obj.from) > 0),
                      "Memo is encrypted to a key ${to} or ${from} not in this wallet.",
                      ("to", memo_obj.to)("from", memo_obj.from));

            std::string message;

            if (_keys.count(memo_obj.to) > 0)
            {
               auto my_key = wif_to_key(_keys.at(memo_obj.to));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               message = memo_obj.get_message(*my_key, memo_obj.from);
               //std::cout << " -- Memo: " << message << std::endl;
            }
            else
            {
               auto my_key = wif_to_key(_keys.at(memo_obj.from));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               message = memo_obj.get_message(*my_key, memo_obj.to);
               //std::cout << " -- Memo: " << message << std::endl;
            }

            fc::mutable_variant_object op_memo;
            op_memo.set("from", op_root["memo"]["from"].as_string());
            op_memo.set("to", op_root["memo"]["to"].as_string());
            op_memo.set("nonce", memo_obj.nonce);
            op_memo.set("message", message);

            op.set("memo", op_memo);
         }
         catch (const fc::exception &e)
         {
            std::cout << " -- could not decrypt memo" << std::endl;
            elog("Error when decrypting memo: ${e}", ("e", e.to_detail_string()));
         }
      }
      ///////////////////

      fc::mutable_variant_object::iterator iter_op = res.find("op");
      iter_op->set(op);

      return res;
   }
  
   string wallet_api_impl::get_operation_id_after_transfer()
   {
      std::string result;

      if (_last_wallet_transfer.is_empty()) { return result; }

      chain::account_id_type account_id = get_account(_last_wallet_transfer._from).get_id();
      fc::optional<asset_object> asset_obj = get_asset(_last_wallet_transfer._asset_symbol);
      bool memo_exists = true;

      std::ostringstream os;
      if (_last_wallet_transfer._memo.length() > 0) {
         os << "Transfer (.+?) " << asset_obj->symbol << " from (.+?) to (.+?) \\-\\- Memo: (.+?)  .+";
      }
      else
      {
         memo_exists = false;
         os << "Transfer (.+?) " << asset_obj->symbol << " from (.+?) to (.+?)  .+";
      }
      std::regex pieces_regex(os.str());

      int i = 30;
      while (i > 0)
      {
         std::this_thread::sleep_for(std::chrono::seconds(1));

         vector<operation_history_object> current = _remote_hist->get_account_history(account_id,
                                                                                      operation_history_id_type(), 5,
                                                                                      operation_history_id_type());

         for (operation_history_object& obj: current)
         {
            std::stringstream ss;
            //const std::string& memo = obj.op.visit(detail::operation_printer(ss, *this, obj.result));
            //obj.op.visit(detail::operation_printer(ss, *this, obj.result));

            std::smatch pieces_match;
            const std::string& source = ss.str();
            // std::cout << source << std::endl;

            if (std::regex_match(source, pieces_match, pieces_regex))
            {
               size_t p_size = memo_exists ? 5: 4;

               if (pieces_match.size() == p_size)
               {
                  const std::string& amount = std::string(pieces_match[1]);
                  const std::string& from = std::string(pieces_match[2]);
                  const std::string& to = std::string(pieces_match[3]);
                  const std::string& memo = (memo_exists) ? std::string(pieces_match[4]): "";

                  if ((asset_obj->amount_from_string(amount) ==
                       asset_obj->amount_from_string(_last_wallet_transfer._amount)) &&
                      (from == _last_wallet_transfer._from) && (to == _last_wallet_transfer._to) &&
                      (memo == _last_wallet_transfer._memo))
                  {
                     result = std::string(obj.id);
                     break;
                  }
               }
            }
         }

         if (!result.empty())
         {
            _last_wallet_transfer.reset();
            break;
         }
         --i;
      }

      return result;
   }

    void wallet_api_impl::on_block_applied(const variant& block_id) {
      fc::async([this]{resync();}, "Resync after block");
   }
    
    bool wallet_api_impl::copy_wallet_file(string destination_filename)
   {
      fc::path src_path = get_wallet_filename();
      if (!fc::exists(src_path)) {
         return false;
      }
      fc::path dest_path = destination_filename + _wallet_filename_extension;
      int suffix = 0;
      while (fc::exists(dest_path))
      {
         ++suffix;
         dest_path = destination_filename + "-" + to_string( suffix ) + _wallet_filename_extension;
      }
      wlog( "backing up wallet ${src} to ${dest}",
            ("src", src_path)
            ("dest", dest_path) );

      fc::path dest_parent = fc::absolute(dest_path).parent_path();
      try
      {
         enable_umask_protection();
         if (!fc::exists(dest_parent)) {
            fc::create_directories(dest_parent);
         }
         fc::copy( src_path, dest_path );
         disable_umask_protection();
      }
      catch(...)
      {
         disable_umask_protection();
         throw;
      }
      return true;
   }

   /***
    * @brief returns true if the wallet is unlocked
    */
   bool wallet_api_impl::is_locked() const {
      return _checksum == fc::sha512();
   }
   
   void wallet_api_impl::set_operation_fees(signed_transaction& tx, const fee_schedule& s)
   {
      asset_object edc_asset = get_asset(EDC_ASSET_SYMBOL);
      for (auto& op: tx.operations) {
         s.set_fee(op, edc_asset.options.core_exchange_rate);
      }
   }

   void wallet_api_impl::set_operation_fees(signed_transaction& tx, const fee_schedule& s, price& ex_rate)
   {
      for (auto& op : tx.operations) {
         s.set_fee(op, ex_rate);
      }
   }
   
   fc::variant wallet_api_impl::info() const
   {
      auto chain_props = get_chain_properties();
      auto global_props = get_global_properties();
      auto dynamic_props = get_dynamic_global_properties();
      fc::mutable_variant_object result;
      result["chain_id"] = chain_props.chain_id;
      result["immutable_parameters"] = fc::variant(chain_props.immutable_parameters, 1);
      result["head_block_num"] = dynamic_props.head_block_number;
      result["head_block_id"] = fc::variant(dynamic_props.head_block_id, 1);
      result["head_block_age"] = fc::get_approximate_relative_time_string(dynamic_props.time,
                                                                          time_point_sec(time_point::now()),
                                                                          " old");
      result["next_maintenance_time"] = fc::time_point_sec(dynamic_props.next_maintenance_time).to_iso_string()
                                        + " (" + fc::get_approximate_relative_time_string(dynamic_props.next_maintenance_time) + ")";

      stringstream participation;
      participation << std::fixed << std::setprecision(2) << (100.0*fc::popcount(dynamic_props.recent_slots_filled)) / 128.0;
      result["participation"] = participation.str();

      result["active_witnesses"] = fc::variant(global_props.active_witnesses, GRAPHENE_MAX_NESTED_OBJECTS);
      result["active_committee_members"] = fc::variant(global_props.active_committee_members, GRAPHENE_MAX_NESTED_OBJECTS);

      return result;
   }

    variant_object wallet_api_impl::about() const
   {
      string client_version( graphene::utilities::git_revision_description );
      const size_t pos = client_version.find( '/' );
      if (pos != string::npos && client_version.size() > pos) {
         client_version = client_version.substr(pos + 1);
      }

      fc::mutable_variant_object result;
      //result["blockchain_name"]        = BLOCKCHAIN_NAME;
      //result["blockchain_description"] = BTS_BLOCKCHAIN_DESCRIPTION;
      result["client_version"]           = client_version;
      result["graphene_revision"]        = graphene::utilities::git_revision_sha;
      result["graphene_revision_age"]    = fc::get_approximate_relative_time_string( fc::time_point_sec( graphene::utilities::git_revision_unix_timestamp ) );
      result["fc_revision"]              = fc::git_revision_sha;
      result["fc_revision_age"]          = fc::get_approximate_relative_time_string( fc::time_point_sec( fc::git_revision_unix_timestamp ) );
      result["compile_date"]             = "compiled on " __DATE__ " at " __TIME__;
      result["boost_version"]            = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
      result["openssl_version"]          = OPENSSL_VERSION_TEXT;

      std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
      std::string os = "osx";
#elif defined(__linux__)
      std::string os = "linux";
#elif defined(_MSC_VER)
      std::string os = "win32";
#else
      std::string os = "other";
#endif
      result["build"] = os + " " + bitness;

      return result;
   }

   chain_property_object wallet_api_impl::get_chain_properties() const {
      return _remote_db->get_chain_properties();
   }

   global_property_object wallet_api_impl::get_global_properties() const {
      return _remote_db->get_global_properties();
   }

   dynamic_global_property_object wallet_api_impl::get_dynamic_global_properties() const {
      return _remote_db->get_dynamic_global_properties();
   }

   string wallet_api_impl::get_wallet_filename() const {
      return _wallet_filename;
   }

   bool wallet_api_impl::load_wallet_file(string wallet_filename)
   {
      // TODO:  Merge imported wallet with existing wallet,
      //        instead of replacing it
      if (wallet_filename == "") {
         wallet_filename = _wallet_filename;
      }

      if (!fc::exists( wallet_filename)) {
         return false;
      }

      _wallet = fc::json::from_file( wallet_filename ).as< wallet_data >( 2 * GRAPHENE_MAX_NESTED_OBJECTS );

      if( _wallet.chain_id != _chain_id )
         FC_THROW( "Wallet chain ID does not match",
            ("wallet.chain_id", _wallet.chain_id)
            ("chain_id", _chain_id) );

      size_t account_pagination = 100;
      vector< account_id_type > account_ids_to_send;
      size_t n = _wallet.my_accounts.size();
      account_ids_to_send.reserve( std::min( account_pagination, n ) );
      auto it = _wallet.my_accounts.begin();

      for( size_t start=0; start<n; start+=account_pagination )
      {
         size_t end = std::min( start+account_pagination, n );
         assert( end > start );
         account_ids_to_send.clear();
         std::vector< account_object > old_accounts;
         for( size_t i=start; i<end; i++ )
         {
            assert( it != _wallet.my_accounts.end() );
            old_accounts.push_back( *it );
            account_ids_to_send.push_back( old_accounts.back().id );
            ++it;
         }
         std::vector< fc::optional< account_object > > accounts = _remote_db->get_accounts(account_ids_to_send);
         // server response should be same length as request
         FC_ASSERT( accounts.size() == account_ids_to_send.size() );
         size_t i = 0;
         for( const fc::optional< account_object >& acct : accounts )
         {
            account_object& old_acct = old_accounts[i];
            if( !acct.valid() )
            {
               elog( "Could not find account ${id} : \"${name}\" does not exist on the chain!", ("id", old_acct.id)("name", old_acct.name) );
               i++;
               continue;
            }
            // this check makes sure the server didn't send results
            // in a different order, or accounts we didn't request
            FC_ASSERT( acct->id == old_acct.id );
            if( fc::json::to_string(*acct) != fc::json::to_string(old_acct) )
            {
               wlog( "Account ${id} : \"${name}\" updated on chain", ("id", acct->id)("name", acct->name) );
            }
            _wallet.update_account( *acct );
            i++;
         }
      }

      return true;
   }

   void wallet_api_impl::save_wallet_file(string wallet_filename)
   {
      //
      // Serialize in memory, then save to disk
      //
      // This approach lessens the risk of a partially written wallet
      // if exceptions are thrown in serialization
      //

      encrypt_keys();

      if( wallet_filename == "" )
         wallet_filename = _wallet_filename;

      wlog( "saving wallet to file ${fn}", ("fn", wallet_filename) );

      string data = fc::json::to_pretty_string( _wallet );
      try
      {
         enable_umask_protection();
         //
         // Parentheses on the following declaration fails to compile,
         // due to the Most Vexing Parse.  Thanks, C++
         //
         // http://en.wikipedia.org/wiki/Most_vexing_parse
         //
         fc::ofstream outfile{ fc::path( wallet_filename ) };
         outfile.write( data.c_str(), data.length() );
         outfile.flush();
         outfile.close();
         disable_umask_protection();
      }
      catch(...)
      {
         disable_umask_protection();
         throw;
      }
   }
   
   operation wallet_api_impl::get_prototype_operation( string operation_name )
   {
      auto it = _prototype_ops.find( operation_name );
      if( it == _prototype_ops.end() )
         FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
      return it->second;
   }
   
   void wallet_api_impl::init_prototype_ops()
   {
      operation op;
      for (size_t t=0; t<op.count(); t++)
      {
         op.set_which( t );
         op.visit( op_prototype_visitor(t, _prototype_ops) );
      }
      return;
   }
   
   int wallet_api_impl::find_first_unused_derived_key_index(const fc::ecc::private_key& parent_key)
   {
      int first_unused_index = 0;
      int number_of_consecutive_unused_keys = 0;
      for (int key_index = 0; ; ++key_index)
      {
         fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key), key_index);
         graphene::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
         if( _keys.find(derived_public_key) == _keys.end() )
         {
            if (number_of_consecutive_unused_keys)
            {
               ++number_of_consecutive_unused_keys;
               if (number_of_consecutive_unused_keys > 5)
                  return first_unused_index;
            }
            else
            {
               first_unused_index = key_index;
               number_of_consecutive_unused_keys = 1;
            }
         }
         else
         {
            // key_index is used
            first_unused_index = 0;
            number_of_consecutive_unused_keys = 0;
         }
      }
   }   
   
   void wallet_api_impl::enable_umask_protection()
   {
#ifdef __unix__
      _old_umask = umask( S_IRWXG | S_IRWXO );
#endif
   }

   void wallet_api_impl::disable_umask_protection()
   {
#ifdef __unix__
      umask( _old_umask );
#endif
   }
   
   void wallet_api_impl::resync()
   {
      fc::scoped_lock<fc::mutex> lock(_resync_mutex);
      // this method is used to update wallet_data annotations
      //   e.g. wallet has been restarted and was not notified
      //   of events while it was down
      //
      // everything that is done "incremental style" when a push
      //   notification is received, should also be done here
      //   "batch style" by querying the blockchain

      if (!_wallet.pending_account_registrations.empty())
      {
         // make a vector of the account names pending registration
         std::vector<string> pending_account_names = boost::copy_range<std::vector<string> >(boost::adaptors::keys(_wallet.pending_account_registrations));

         // look those up on the blockchain
         std::vector<fc::optional<graphene::chain::account_object >>
               pending_account_objects = _remote_db->lookup_account_names( pending_account_names );

         // if any of them exist, claim them
         for( const fc::optional<graphene::chain::account_object>& optional_account : pending_account_objects )
            if( optional_account )
               claim_registered_account(*optional_account);
      }

      if (!_wallet.pending_witness_registrations.empty())
      {
         // make a vector of the owner accounts for witnesses pending registration
         std::vector<string> pending_witness_names = boost::copy_range<std::vector<string> >(boost::adaptors::keys(_wallet.pending_witness_registrations));

         // look up the owners on the blockchain
         std::vector<fc::optional<graphene::chain::account_object>> owner_account_objects = _remote_db->lookup_account_names(pending_witness_names);

         // if any of them have registered witnesses, claim them
         for( const fc::optional<graphene::chain::account_object>& optional_account : owner_account_objects )
            if (optional_account)
            {
               fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(optional_account->id);
               if (witness_obj)
                  claim_registered_witness(optional_account->name);
            }
      }
   }

}}} // namespace graphene::wallet::detail
