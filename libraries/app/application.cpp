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
#include <graphene/app/api.hpp>
#include <graphene/app/api_access.hpp>
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/protocol/types.hpp>
#include <graphene/chain/worker_evaluator.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/exceptions.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include <fc/asio.hpp>
#include <fc/io/fstream.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/network/resolve.hpp>

#include <fc/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/signals2.hpp>
#include <boost/range/algorithm/reverse.hpp>

#include <iostream>
#include <sstream>
#include <tuple>

#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#include <boost/range/adaptor/reversed.hpp>

namespace graphene { namespace app {
using net::item_hash_t;
using net::item_id;
using net::message;
using net::block_message;
using net::trx_message;

using chain::block_header;
using chain::signed_block_header;
using chain::signed_block;
using chain::block_id_type;

using std::vector;

namespace bpo = boost::program_options;

namespace detail {

   genesis_state_type create_example_genesis()
   {
      auto nathan_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      dlog("Allocating all stake to ${key}", ("key", utilities::key_to_wif(nathan_key)));
      genesis_state_type initial_state;
      initial_state.initial_parameters.get_mutable_fees() = fee_schedule::get_default();
      initial_state.initial_active_witnesses = GRAPHENE_DEFAULT_MIN_WITNESS_COUNT;
      initial_state.initial_timestamp = time_point_sec(time_point::now().sec_since_epoch() /
            initial_state.initial_parameters.block_interval *
            initial_state.initial_parameters.block_interval);
      for( uint64_t i = 0; i < initial_state.initial_active_witnesses; ++i )
      {
         auto name = "init"+fc::to_string(i);
         initial_state.initial_accounts.emplace_back(name,
                                                     nathan_key.get_public_key(),
                                                     nathan_key.get_public_key(),
                                                     true);
         initial_state.initial_committee_candidates.push_back({name});
         initial_state.initial_witness_candidates.push_back({name, nathan_key.get_public_key()});
      }

      initial_state.initial_accounts.emplace_back("nathan", nathan_key.get_public_key());
      initial_state.initial_balances.push_back({nathan_key.get_public_key(),
                                                GRAPHENE_SYMBOL,
                                                GRAPHENE_MAX_SHARE_SUPPLY});
      initial_state.initial_chain_id = fc::sha256::hash( "BOGUS" );

      return initial_state;
   }

} } } // namespace graphene::app::detail

#include "application_impl.hxx"
   
namespace graphene { namespace app { namespace detail {
   
   void application_impl::reset_p2p_node(const fc::path& data_dir)
   { try {
      _p2p_network = std::make_shared<net::node>("Graphene Reference Implementation");

      _p2p_network->load_configuration(data_dir / "p2p");
      _p2p_network->set_node_delegate(this);

      if( _options->count("seed-node") )
      {
         auto seeds = _options->at("seed-node").as<vector<string>>();
         for( const string& endpoint_string : seeds )
         {
            std::vector<fc::ip::endpoint> endpoints = resolve_string_to_ip_endpoints(endpoint_string);
            for (const fc::ip::endpoint& endpoint : endpoints)
            {
               ilog("Adding seed node ${endpoint}", ("endpoint", endpoint));
               _p2p_network->add_node(endpoint);
               _p2p_network->connect_to_endpoint(endpoint);
            }
         }
      }

      if( _options->count("p2p-endpoint") )
         _p2p_network->listen_on_endpoint(fc::ip::endpoint::from_string(_options->at("p2p-endpoint").as<string>()), true);
      else
         _p2p_network->listen_on_port(0, false);
      _p2p_network->listen_to_p2p_network();
      ilog("Configured p2p node to listen on ${ip}", ("ip", _p2p_network->get_actual_listening_endpoint()));

      _p2p_network->connect_to_p2p_network();
      _p2p_network->sync_from(net::item_id(net::core_message_type_enum::block_message_type,
                                           _chain_db->head_block_id()),
                              std::vector<uint32_t>());
   } FC_CAPTURE_AND_RETHROW() }

   std::vector<fc::ip::endpoint> application_impl::resolve_string_to_ip_endpoints(const std::string& endpoint_string)
   {
      try
      {
         string::size_type colon_pos = endpoint_string.find(':');
         if (colon_pos == std::string::npos)
            FC_THROW("Missing required port number in endpoint string \"${endpoint_string}\"",
                     ("endpoint_string", endpoint_string));
         std::string port_string = endpoint_string.substr(colon_pos + 1);
         try
         {
            uint16_t port = boost::lexical_cast<uint16_t>(port_string);

            std::string hostname = endpoint_string.substr(0, colon_pos);
            std::vector<fc::ip::endpoint> endpoints = fc::resolve(hostname, port);
            if (endpoints.empty())
               FC_THROW_EXCEPTION(fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname));
            return endpoints;
         }
         catch (const boost::bad_lexical_cast&)
         {
            FC_THROW("Bad port: ${port}", ("port", port_string));
         }
      }
      FC_CAPTURE_AND_RETHROW((endpoint_string))
   }

   void application_impl::reset_websocket_server()
   { try {
      if( !_options->count("rpc-endpoint") )
         return;

      //bool enable_deflate_compression = _options->count("enable-permessage-deflate") != 0;

      _websocket_server = std::make_shared<fc::http::websocket_server>("");

      _websocket_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
         auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(c, GRAPHENE_NET_MAX_NESTED_OBJECTS);
         auto login = std::make_shared<graphene::app::login_api>( std::ref(*_self) );
         auto db_api = std::make_shared<graphene::app::database_api>( std::ref(*_self->chain_database()) );
         wsc->register_api(fc::api<graphene::app::database_api>(db_api));
         wsc->register_api(fc::api<graphene::app::login_api>(login));
         c->set_session_data( wsc );
      });
      ilog("Configured websocket rpc to listen on ${ip}", ("ip",_options->at("rpc-endpoint").as<string>()));
      _websocket_server->listen( fc::ip::endpoint::from_string(_options->at("rpc-endpoint").as<string>()) );
      _websocket_server->start_accept();
   } FC_CAPTURE_AND_RETHROW() }

   void application_impl::reset_websocket_tls_server()
   { try {
      if( !_options->count("rpc-tls-endpoint") )
         return;
      if( !_options->count("server-pem") )
      {
         wlog( "Please specify a server-pem to use rpc-tls-endpoint" );
         return;
      }

      string password = _options->count("server-pem-password") ? _options->at("server-pem-password").as<string>() : "";
      //bool enable_deflate_compression = _options->count("enable-permessage-deflate") != 0;
      _websocket_tls_server = std::make_shared<fc::http::websocket_tls_server>(_options->at("server-pem").as<string>(), password, "");

      _websocket_tls_server->on_connection([&]( const fc::http::websocket_connection_ptr& c )
      {
         auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(c, GRAPHENE_NET_MAX_NESTED_OBJECTS);
         auto login = std::make_shared<graphene::app::login_api>( std::ref(*_self) );
         auto db_api = std::make_shared<graphene::app::database_api>( std::ref(*_self->chain_database()) );
         wsc->register_api(fc::api<graphene::app::database_api>(db_api));
         wsc->register_api(fc::api<graphene::app::login_api>(login));
         c->set_session_data( wsc );
      });
      ilog("Configured websocket TLS rpc to listen on ${ip}", ("ip",_options->at("rpc-tls-endpoint").as<string>()));
      _websocket_tls_server->listen( fc::ip::endpoint::from_string(_options->at("rpc-tls-endpoint").as<string>()) );
      _websocket_tls_server->start_accept();
   } FC_CAPTURE_AND_RETHROW() }

   void application_impl::set_dbg_init_key( genesis_state_type& genesis, const std::string& init_key )
   {
      flat_set< std::string > initial_witness_names;
      public_key_type init_pubkey( init_key );
      for( uint64_t i=0; i<genesis.initial_active_witnesses; i++ )
         genesis.initial_witness_candidates[i].block_signing_key = init_pubkey;
   }

   void application_impl::startup()
   { try {
      bool clean = !fc::exists(_data_dir / "blockchain/dblock");
      fc::create_directories(_data_dir / "blockchain/dblock");

      auto initial_state = [this] {
         ilog("Initializing database...");
         FC_ASSERT(_options->count("genesis-json"), "you must specify genesis-file");
         if( _options->count("genesis-json") )
         {
            const boost::filesystem::path& gfile = _options->at("genesis-json").as<boost::filesystem::path>();
            FC_ASSERT(fc::exists(gfile), "genesis file '${f}' not exists", ("f", (gfile.string())));

            std::string genesis_str;
            fc::read_file_contents( gfile, genesis_str );
            genesis_state_type genesis = fc::json::from_string( genesis_str ).as<genesis_state_type>(20);
            bool modified_genesis = false;
            if( _options->count("genesis-timestamp") )
            {
               genesis.initial_timestamp = fc::time_point_sec( fc::time_point::now() ) + genesis.initial_parameters.block_interval + _options->at("genesis-timestamp").as<uint32_t>();
               genesis.initial_timestamp -= genesis.initial_timestamp.sec_since_epoch() % genesis.initial_parameters.block_interval;
               modified_genesis = true;
               std::cerr << "Used genesis timestamp:  " << genesis.initial_timestamp.to_iso_string() << " (PLEASE RECORD THIS)\n";
            }
            if( _options->count("dbg-init-key") )
            {
               std::string init_key = _options->at( "dbg-init-key" ).as<string>();
               FC_ASSERT( genesis.initial_witness_candidates.size() >= genesis.initial_active_witnesses );
               set_dbg_init_key( genesis, init_key );
               modified_genesis = true;
               std::cerr << "Set init witness key to " << init_key << "\n";
            }
            if( modified_genesis )
            {
               std::cerr << "WARNING:  GENESIS WAS MODIFIED, YOUR CHAIN ID MAY BE DIFFERENT\n";
               genesis_str += "BOGUS";
               genesis.initial_chain_id = fc::sha256::hash( genesis_str );
            }
            else
               genesis.initial_chain_id = fc::sha256::hash( genesis_str );
            return genesis;
         }
         else
         {
            std::string egenesis_json;
            graphene::egenesis::compute_egenesis_json( egenesis_json );
            FC_ASSERT( egenesis_json != "" );
            FC_ASSERT( graphene::egenesis::get_egenesis_json_hash() == fc::sha256::hash( egenesis_json ) );
            auto genesis = fc::json::from_string( egenesis_json ).as<genesis_state_type>(20);
            genesis.initial_chain_id = fc::sha256::hash( egenesis_json );
            return genesis;
         }
      };

      if( _options->count("resync-blockchain") ) {
       _chain_db->wipe(_data_dir / "blockchain", true);
      }

      flat_map<uint32_t,block_id_type> loaded_checkpoints;
      if( _options->count("checkpoint") )
      {
         auto cps = _options->at("checkpoint").as<vector<string>>();
         loaded_checkpoints.reserve( cps.size() );
         for( auto cp : cps )
         {
            auto item = fc::json::from_string(cp).as<std::pair<uint32_t,block_id_type> >(2);
            loaded_checkpoints[item.first] = item.second;
         }
      }
      _chain_db->add_checkpoints( loaded_checkpoints );

      if( _options->count("replay-blockchain") )
      {
         ilog("Replaying blockchain on user request.");
         _chain_db->reindex(_data_dir/"blockchain", initial_state());
      } else if( clean ) {

         auto is_new = [&]() -> bool
         {
            // directory doesn't exist
            if( !fc::exists( _data_dir ) )
               return true;
            // if directory exists but is empty, return true; else false.
            return ( fc::directory_iterator( _data_dir ) == fc::directory_iterator() );
         };

         auto is_outdated = [&]() -> bool
         {
            if( !fc::exists( _data_dir / "db_version" ) )
               return true;
            std::string version_str;
            fc::read_file_contents( _data_dir / "db_version", version_str );
            return (version_str != GRAPHENE_CURRENT_DB_VERSION);
         };

         bool need_reindex = (!is_new() && is_outdated());
         std::string reindex_reason = "version upgrade";

         if( !need_reindex )
         {
            try {
               _chain_db->open(_data_dir / "blockchain", initial_state);
            }
            catch( const fc::exception& e )
            {
               ilog( "caught exception ${e} in open()", ("e", e.to_detail_string()) );
               need_reindex = true;
               reindex_reason = "exception in open()";
            }
         }

         if( need_reindex )
         {
            ilog("Replaying blockchain due to ${reason}", ("reason", reindex_reason) );

            fc::remove_all( _data_dir / "db_version" );
            _chain_db->reindex(_data_dir / "blockchain", initial_state());

            // doing this down here helps ensure that DB will be wiped
            // if any of the above steps were interrupted on a previous run
            if( !fc::exists( _data_dir / "db_version" ) )
            {
               std::ofstream db_version(
                  (_data_dir / "db_version").generic_string().c_str(),
                  std::ios::out | std::ios::binary | std::ios::trunc );
               std::string version_string = GRAPHENE_CURRENT_DB_VERSION;
               db_version.write( version_string.c_str(), version_string.size() );
               db_version.close();
            }
         }
      } else {
         wlog("Detected unclean shutdown. Replaying blockchain...");
         _chain_db->reindex(_data_dir / "blockchain", initial_state());
      }

      if (!_options->count("genesis-json") &&
          _chain_db->get_chain_id() != graphene::egenesis::get_egenesis_chain_id()) {
         elog("Detected old database. Nuking and starting over.");
         _chain_db->wipe(_data_dir / "blockchain", true);
         _chain_db.reset();
         _chain_db = std::make_shared<chain::database>();
         _chain_db->add_checkpoints(loaded_checkpoints);
         _chain_db->open(_data_dir / "blockchain", initial_state);
      }

      if( _options->count("force-validate") )
      {
         ilog( "All transaction signatures will be validated" );
         _force_validate = true;
      }

      if ( _options->count("api-access") ) {
         _apiaccess = fc::json::from_file(_options->at("api-access").as<boost::filesystem::path>()).as<api_access>(20);
      }
      else
      {
         // TODO:  Remove this generous default access policy
         // when the UI logs in properly
         _apiaccess = api_access();
         api_access_info wild_access;
         wild_access.password_hash_b64 = "*";
         wild_access.password_salt_b64 = "*";
         wild_access.allowed_apis.push_back( "database_api" );
         wild_access.allowed_apis.push_back( "network_broadcast_api" );
         wild_access.allowed_apis.push_back( "history_api" );
         wild_access.allowed_apis.push_back( "crypto_api" );
         _apiaccess.permission_map["*"] = wild_access;
      }

      reset_p2p_node(_data_dir);
      reset_websocket_server();
      reset_websocket_tls_server();
   } FC_LOG_AND_RETHROW() }

   fc::optional< api_access_info > application_impl::get_api_access_info(const string& username)const
   {
      fc::optional< api_access_info > result;
      auto it = _apiaccess.permission_map.find(username);
      if( it == _apiaccess.permission_map.end() )
      {
         it = _apiaccess.permission_map.find("*");
         if( it == _apiaccess.permission_map.end() )
            return result;
      }
      return it->second;
   }

   void application_impl::set_api_access_info(const string& username, api_access_info&& permissions) {
      _apiaccess.permission_map.insert(std::make_pair(username, std::move(permissions)));
   }

   /**
    * If delegate has the item, the network has no need to fetch it.
    */
   bool application_impl::has_item(const net::item_id& id)
   {
      try
      {
         if( id.item_type == graphene::net::block_message_type )
            return _chain_db->is_known_block(id.item_hash);
         else
            return _chain_db->is_known_transaction(id.item_hash);
      }
      FC_CAPTURE_AND_RETHROW( (id) )
   }

   /**
    * @brief allows the application to validate an item prior to broadcasting to peers.
    *
    * @param sync_mode true if the message was fetched through the sync process, false during normal operation
    * @returns true if this message caused the blockchain to switch forks, false if it did not
    *
    * @throws exception if error validating the item, otherwise the item is safe to broadcast on.
    */
   bool application_impl::handle_block(const graphene::net::block_message& blk_msg, bool sync_mode,
                             std::vector<fc::uint160_t>& contained_transaction_message_ids)
   { try {

      auto latency = fc::time_point::now() - blk_msg.block.timestamp;
      if (!sync_mode || blk_msg.block.block_num() % 10000 == 0)
      {
         const auto& witness = blk_msg.block.witness(*_chain_db);
         const auto& witness_account = witness.witness_account(*_chain_db);
         auto last_irr = _chain_db->get_dynamic_global_properties().last_irreversible_block_num;
         ilog("Got block: #${n} time: ${t} latency: ${l} ms from: ${w}  irreversible: ${i} (-${d})",
              ("t",blk_msg.block.timestamp)
              ("n", blk_msg.block.block_num())
              ("l", (latency.count()/1000))
              ("w",witness_account.name)
              ("i",last_irr)("d",blk_msg.block.block_num()-last_irr) );
      }

      try {
         // TODO: in the case where this block is valid but on a fork that's too old for us to switch to,
         // you can help the network code out by throwing a block_older_than_undo_history exception.
         // when the net code sees that, it will stop trying to push blocks from that chain, but
         // leave that peer connected so that they can get sync blocks from us
         bool result = _chain_db->push_block(blk_msg.block, (_is_block_producer | _force_validate) ? database::skip_nothing : database::skip_transaction_signatures);

         // the block was accepted, so we now know all of the transactions contained in the block
         if (!sync_mode)
         {
            // if we're not in sync mode, there's a chance we will be seeing some transactions
            // included in blocks before we see the free-floating transaction itself.  If that
            // happens, there's no reason to fetch the transactions, so  construct a list of the
            // transaction message ids we no longer need.
            // during sync, it is unlikely that we'll see any old
            for (const processed_transaction& transaction : blk_msg.block.transactions)
            {
               graphene::net::trx_message transaction_message(transaction);
               contained_transaction_message_ids.push_back(graphene::net::message(transaction_message).id());
            }
         }

         return result;
      } catch ( const graphene::chain::unlinkable_block_exception& e ) {
         // translate to a graphene::net exception
         elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
         FC_THROW_EXCEPTION(graphene::net::unlinkable_block_exception, "Error when pushing block:\n${e}", ("e", e.to_detail_string()));
      } catch( const fc::exception& e ) {
         elog("Error when pushing block:\n${e}", ("e", e.to_detail_string()));
         throw;
      }

      if( !_is_finished_syncing && !sync_mode )
      {
         _is_finished_syncing = true;
         _self->syncing_finished();
      }
   } FC_CAPTURE_AND_RETHROW( (blk_msg)(sync_mode) ) }

   void application_impl::handle_transaction(const graphene::net::trx_message& transaction_message)
   { try {
      static fc::time_point last_call;
      static int trx_count = 0;
      ++trx_count;
      auto now = fc::time_point::now();
      if( now - last_call > fc::seconds(1) ) {
         ilog("Got ${c} transactions from network", ("c",trx_count) );
         last_call = now;
         trx_count = 0;
      }

      _chain_db->push_transaction( transaction_message.trx );
   } FC_CAPTURE_AND_RETHROW( (transaction_message) ) }

   void application_impl::handle_message(const message& message_to_process)
   {
      // not a transaction, not a block
      FC_THROW( "Invalid Message Type" );
   }

   bool application_impl::is_included_block(const block_id_type& block_id)
   {
     uint32_t block_num = block_header::num_from_id(block_id);
     block_id_type block_id_in_preferred_chain = _chain_db->get_block_id_for_num(block_num);
     return block_id == block_id_in_preferred_chain;
   }

   /**
    * Assuming all data elements are ordered in some way, this method should
    * return up to limit ids that occur *after* the last ID in synopsis that
    * we recognize.
    *
    * On return, remaining_item_count will be set to the number of items
    * in our blockchain after the last item returned in the result,
    * or 0 if the result contains the last item in the blockchain
    */
   std::vector<item_hash_t> application_impl::get_block_ids(
      const std::vector<item_hash_t>& blockchain_synopsis,
      uint32_t& remaining_item_count,
      uint32_t limit)
   { try {
      vector<block_id_type> result;
      remaining_item_count = 0;
      if( _chain_db->head_block_num() == 0 )
         return result;

      result.reserve(limit);
      block_id_type last_known_block_id;

      if (blockchain_synopsis.empty() ||
          (blockchain_synopsis.size() == 1 && blockchain_synopsis[0] == block_id_type()))
      {
        // peer has sent us an empty synopsis meaning they have no blocks.
        // A bug in old versions would cause them to send a synopsis containing block 000000000
        // when they had an empty blockchain, so pretend they sent the right thing here.

        // do nothing, leave last_known_block_id set to zero
      }
      else
      {
        bool found_a_block_in_synopsis = false;
        for (const item_hash_t& block_id_in_synopsis : boost::adaptors::reverse(blockchain_synopsis))
          if (block_id_in_synopsis == block_id_type() ||
              (_chain_db->is_known_block(block_id_in_synopsis) && is_included_block(block_id_in_synopsis)))
          {
            last_known_block_id = block_id_in_synopsis;
            found_a_block_in_synopsis = true;
            break;
          }
        if (!found_a_block_in_synopsis)
          FC_THROW_EXCEPTION(graphene::net::peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
      }
      for( uint32_t num = block_header::num_from_id(last_known_block_id);
           num <= _chain_db->head_block_num() && result.size() < limit;
           ++num )
         if( num > 0 )
            result.push_back(_chain_db->get_block_id_for_num(num));

      if( !result.empty() && block_header::num_from_id(result.back()) < _chain_db->head_block_num() )
         remaining_item_count = _chain_db->head_block_num() - block_header::num_from_id(result.back());

      return result;
   } FC_CAPTURE_AND_RETHROW( (blockchain_synopsis)(remaining_item_count)(limit) ) }

   /**
    * Given the hash of the requested data, fetch the body.
    */
   message application_impl::get_item(const item_id& id)
   { try {
     // ilog("Request for item ${id}", ("id", id));
      if( id.item_type == graphene::net::block_message_type )
      {
         auto opt_block = _chain_db->fetch_block_by_id(id.item_hash);
         if( !opt_block )
            elog("Couldn't find block ${id} -- corresponding ID in our chain is ${id2}",
                 ("id", id.item_hash)("id2", _chain_db->get_block_id_for_num(block_header::num_from_id(id.item_hash))));
         FC_ASSERT( opt_block.valid() );
         // ilog("Serving up block #${num}", ("num", opt_block->block_num()));
         return block_message(std::move(*opt_block));
      }
      return trx_message( _chain_db->get_recent_transaction( id.item_hash ) );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   chain_id_type application_impl::get_chain_id()const {
      return _chain_db->get_chain_id();
   }

   /**
    * Returns a synopsis of the blockchain used for syncing.  This consists of a list of
    * block hashes at intervals exponentially increasing towards the genesis block.
    * When syncing to a peer, the peer uses this data to determine if we're on the same
    * fork as they are, and if not, what blocks they need to send us to get us on their
    * fork.
    *
    * In the over-simplified case, this is a straightforward synopsis of our current
    * preferred blockchain; when we first connect up to a peer, this is what we will be sending.
    * It looks like this:
    *   If the blockchain is empty, it will return the empty list.
    *   If the blockchain has one block, it will return a list containing just that block.
    *   If it contains more than one block:
    *     the first element in the list will be the hash of the highest numbered block that
    *         we cannot undo
    *     the second element will be the hash of an item at the half way point in the undoable
    *         segment of the blockchain
    *     the third will be ~3/4 of the way through the undoable segment of the block chain
    *     the fourth will be at ~7/8...
    *       &c.
    *     the last item in the list will be the hash of the most recent block on our preferred chain
    * so if the blockchain had 26 blocks labeled a - z, the synopsis would be:
    *    a n u x z
    * the idea being that by sending a small (<30) number of block ids, we can summarize a huge
    * blockchain.  The block ids are more dense near the end of the chain where because we are
    * more likely to be almost in sync when we first connect, and forks are likely to be short.
    * If the peer we're syncing with in our example is on a fork that started at block 'v',
    * then they will reply to our synopsis with a list of all blocks starting from block 'u',
    * the last block they know that we had in common.
    *
    * In the real code, there are several complications.
    *
    * First, as an optimization, we don't usually send a synopsis of the entire blockchain, we
    * send a synopsis of only the segment of the blockchain that we have undo data for.  If their
    * fork doesn't build off of something in our undo history, we would be unable to switch, so there's
    * no reason to fetch the blocks.
    *
    * Second, when a peer replies to our initial synopsis and gives us a list of the blocks they think
    * we are missing, they only send a chunk of a few thousand blocks at once.  After we get those
    * block ids, we need to request more blocks by sending another synopsis (we can't just say "send me
    * the next 2000 ids" because they may have switched forks themselves and they don't track what
    * they've sent us).  For faster performance, we want to get a fairly long list of block ids first,
    * then start downloading the blocks.
    * The peer doesn't handle these follow-up block id requests any different from the initial request;
    * it treats the synopsis we send as our blockchain and bases its response entirely off that.  So to
    * get the response we want (the next chunk of block ids following the last one they sent us, or,
    * failing that, the shortest fork off of the last list of block ids they sent), we need to construct
    * a synopsis as if our blockchain was made up of:
    *    1. the blocks in our block chain up to the fork point (if there is a fork) or the head block (if no fork)
    *    2. the blocks we've already pushed from their fork (if there's a fork)
    *    3. the block ids they've previously sent us
    * Segment 3 is handled in the p2p code, it just tells us the number of blocks it has (in
    * number_of_blocks_after_reference_point) so we can leave space in the synopsis for them.
    * We're responsible for constructing the synopsis of Segments 1 and 2 from our active blockchain and
    * fork database.  The reference_point parameter is the last block from that peer that has been
    * successfully pushed to the blockchain, so that tells us whether the peer is on a fork or on
    * the main chain.
    */
   std::vector<item_hash_t> application_impl::get_blockchain_synopsis(
      const item_hash_t& reference_point,
      uint32_t number_of_blocks_after_reference_point)
   { try {
       std::vector<item_hash_t> synopsis;
       synopsis.reserve(30);
       uint32_t high_block_num;
       uint32_t non_fork_high_block_num;
       uint32_t low_block_num = _chain_db->last_non_undoable_block_num();
       std::vector<block_id_type> fork_history;

       if (reference_point != item_hash_t())
       {
         // the node is asking for a summary of the block chain up to a specified
         // block, which may or may not be on a fork
         // for now, assume it's not on a fork
         if (is_included_block(reference_point))
         {
           // reference_point is a block we know about and is on the main chain
           uint32_t reference_point_block_num = block_header::num_from_id(reference_point);
           assert(reference_point_block_num > 0);
           high_block_num = reference_point_block_num;
           non_fork_high_block_num = high_block_num;

           if (reference_point_block_num < low_block_num)
           {
             // we're on the same fork (at least as far as reference_point) but we've passed
             // reference point and could no longer undo that far if we diverged after that
             // block.  This should probably only happen due to a race condition where
             // the network thread calls this function, and then immediately pushes a bunch of blocks,
             // then the main thread finally processes this function.
             // with the current framework, there's not much we can do to tell the network
             // thread what our current head block is, so we'll just pretend that
             // our head is actually the reference point.
             // this *may* enable us to fetch blocks that we're unable to push, but that should
             // be a rare case (and correctly handled)
             low_block_num = reference_point_block_num;
           }
         }
         else
         {
           // block is a block we know about, but it is on a fork
           try
           {
             fork_history = _chain_db->get_block_ids_on_fork(reference_point);
             // returns a vector where the last element is the common ancestor with the preferred chain,
             // and the first element is the reference point you passed in
             assert(fork_history.size() >= 2);

             if( fork_history.front() != reference_point )
             {
                edump( (fork_history)(reference_point) );
                assert(fork_history.front() == reference_point);
             }
             block_id_type last_non_fork_block = fork_history.back();
             fork_history.pop_back();  // remove the common ancestor
             boost::reverse(fork_history);

             if (last_non_fork_block == block_id_type()) // if the fork goes all the way back to genesis (does graphene's fork db allow this?)
               non_fork_high_block_num = 0;
             else
               non_fork_high_block_num = block_header::num_from_id(last_non_fork_block);

             high_block_num = non_fork_high_block_num + fork_history.size();
             assert(high_block_num == block_header::num_from_id(fork_history.back()));
           }
           catch (const fc::exception& e)
           {
             // unable to get fork history for some reason.  maybe not linked?
             // we can't return a synopsis of its chain
             elog("Unable to construct a blockchain synopsis for reference hash ${hash}: ${exception}", ("hash", reference_point)("exception", e));
             throw;
           }
           if (non_fork_high_block_num < low_block_num)
           {
             wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
                  "(our chains diverge after block #${non_fork_high_block_num} but only undoable to block #${low_block_num})",
                  ("low_block_num", low_block_num)
                  ("non_fork_high_block_num", non_fork_high_block_num));
             FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is are on a fork I'm unable to switch to");
           }
         }
       }
       else
       {
         // no reference point specified, summarize the whole block chain
         high_block_num = _chain_db->head_block_num();
         non_fork_high_block_num = high_block_num;
         if (high_block_num == 0)
           return synopsis; // we have no blocks
       }

       if ( low_block_num == 0) {
          low_block_num = 1;
       }

       // at this point:
       // low_block_num is the block before the first block we can undo,
       // non_fork_high_block_num is the block before the fork (if the peer is on a fork, or otherwise it is the same as high_block_num)
       // high_block_num is the block number of the reference block, or the end of the chain if no reference provided

       // true_high_block_num is the ending block number after the network code appends any item ids it
       // knows about that we don't
       uint32_t true_high_block_num = high_block_num + number_of_blocks_after_reference_point;
       do
       {
         // for each block in the synopsis, figure out where to pull the block id from.
         // if it's <= non_fork_high_block_num, we grab it from the main blockchain;
         // if it's not, we pull it from the fork history
         if (low_block_num <= non_fork_high_block_num) {
            synopsis.push_back(_chain_db->get_block_id_for_num(low_block_num));
         }
         else
         {
            uint32_t idx = low_block_num - non_fork_high_block_num - 1;
            if (fork_history.size() > idx) {
               synopsis.push_back(fork_history[idx]);
            }
         }
         low_block_num += (true_high_block_num - low_block_num + 2) / 2;
       }
       while (low_block_num <= high_block_num);

       idump((synopsis));
       return synopsis;
   } FC_CAPTURE_AND_RETHROW() }

   /**
    * Call this after the call to handle_message succeeds.
    *
    * @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
    * @param item_count the number of items known to the node that haven't been sent to handle_item() yet.
    *                   After `item_count` more calls to handle_item(), the node will be in sync
    */
   void application_impl::sync_status(uint32_t item_type, uint32_t item_count) {
      // any status reports to GUI go here
   }

   /**
    * Call any time the number of connected peers changes.
    */
   void application_impl::connection_count_changed(uint32_t c) {
     // any status reports to GUI go here
   }

   uint32_t application_impl::get_block_number(const item_hash_t& block_id)
   { try {
      return block_header::num_from_id(block_id);
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   /**
    * Returns the time a block was produced (if block_id = 0, returns genesis time).
    * If we don't know about the block, returns time_point_sec::min()
    */
   fc::time_point_sec application_impl::get_block_time(const item_hash_t& block_id)
   { try {
      auto opt_block = _chain_db->fetch_block_by_id( block_id );
      if( opt_block.valid() ) return opt_block->timestamp;
      return fc::time_point_sec::min();
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   item_hash_t application_impl::get_head_block_id() const {
      return _chain_db->head_block_id();
   }

   uint32_t application_impl::estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const {
      return 0; // there are no forks in graphene
   }

   void application_impl::error_encountered(const std::string& message, const fc::oexception& error) {
      // notify GUI or something cool
   }

   uint8_t application_impl::get_current_block_interval_in_seconds() const {
      return _chain_db->get_global_properties().parameters.block_interval;
   }

} } } // namespace graphene::app::detail

namespace graphene { namespace app {

application::application()
   : my(new detail::application_impl(this)) { }

application::~application()
{
   if( my->_p2p_network )
   {
      my->_p2p_network->close();
      my->_p2p_network.reset();
   }
   if( my->_chain_db )
   {
      my->_chain_db->close();
   }
}

void application::set_program_options(boost::program_options::options_description& command_line_options,
                                      boost::program_options::options_description& configuration_file_options) const
{
   configuration_file_options.add_options()
         ("p2p-endpoint", bpo::value<string>(), "Endpoint for P2P node to listen on")
         ("seed-node,s", bpo::value<vector<string>>()->composing(), "P2P nodes to connect to on startup (may specify multiple times)")
         ("checkpoint,c", bpo::value<vector<string>>()->composing(), "Pairs of [BLOCK_NUM,BLOCK_ID] that should be enforced as checkpoints.")
         ("rpc-endpoint", bpo::value<string>()->implicit_value("127.0.0.1:8090"), "Endpoint for websocket RPC to listen on")
         ("rpc-tls-endpoint", bpo::value<string>()->implicit_value("127.0.0.1:8089"), "Endpoint for TLS websocket RPC to listen on")
         ("enable-permessage-deflate", "Enable support for per-message deflate compression in the websocket servers "
                                       "(--rpc-endpoint and --rpc-tls-endpoint), disabled by default")
         ("server-pem,p", bpo::value<string>()->implicit_value("server.pem"), "The TLS certificate file for this server")
         ("server-pem-password,P", bpo::value<string>()->implicit_value(""), "Password for this certificate")
         ("genesis-json", bpo::value<boost::filesystem::path>(), "File to read Genesis State from")
         ("dbg-init-key", bpo::value<string>(), "Block signing key to use for init witnesses, overrides genesis file")
         ("api-access", bpo::value<boost::filesystem::path>(), "JSON file specifying API permissions")
         ("io-threads", bpo::value<uint16_t>()->implicit_value(0), "Number of IO threads, default to 0 for auto-configuration")
         ("replay-blockchain", "Rebuild object graph by replaying all blocks")
         ;
   command_line_options.add(configuration_file_options);
   command_line_options.add_options()
         ("create-genesis-json", bpo::value<boost::filesystem::path>(),
          "Path to create a Genesis State at. If a well-formed JSON file exists at the path, it will be parsed and any "
          "missing fields in a Genesis State will be added, and any unknown fields will be removed. If no file or an "
          "invalid file is found, it will be replaced with an example Genesis State.")
         ("resync-blockchain", "Delete all blocks and re-sync with network from scratch")
         ("force-validate", "Force validation of all transactions")
         ("genesis-timestamp", bpo::value<uint32_t>(), "Replace timestamp from genesis.json with current time plus this many seconds (experts only!)")
         ;
   command_line_options.add(_cli_options);
   configuration_file_options.add(_cfg_options);
}

void application::initialize(const fc::path& data_dir, const boost::program_options::variables_map& options)
{
   my->_data_dir = data_dir;
   my->_options = &options;

   if (options.count("fast")) {
       my->_chain_db->set_history_size(options.at("fast").as<int>());
   }
   if( options.count("create-genesis-json") )
   {
      fc::path genesis_out = options.at("create-genesis-json").as<boost::filesystem::path>();
      genesis_state_type genesis_state = detail::create_example_genesis();
      if( fc::exists(genesis_out) )
      {
         try {
            genesis_state = fc::json::from_file(genesis_out).as<genesis_state_type>(20);
         } catch(const fc::exception& e) {
            std::cerr << "Unable to parse existing genesis file:\n" << e.to_string()
                      << "\nWould you like to replace it? [y/N] ";
            char response = std::cin.get();
            if( toupper(response) != 'Y' )
               return;
         }

         std::cerr << "Updating genesis state in file " << genesis_out.generic_string() << "\n";
      } else {
         std::cerr << "Creating example genesis state in file " << genesis_out.generic_string() << "\n";
      }
      fc::json::save_to_file(genesis_state, genesis_out);

      std::exit(EXIT_SUCCESS);
   }

   if ( options.count("io-threads") )
   {
      const uint16_t num_threads = options["io-threads"].as<uint16_t>();
      fc::asio::default_io_service_scope::set_num_threads(num_threads);
   }
}

void application::startup()
{
   try {
   my->startup();
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   } catch ( ... ) {
      elog( "unexpected exception" );
      throw;
   }
   if (key_string.empty()) return;

   const auto& edc_asset = my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
   if (edc_asset != my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().end())
   {
      const auto& acc_idx = my->_chain_db->get_index_type<account_index>().indices().get<by_id>();
      auto itr_acc = acc_idx.find(edc_asset->issuer);
      if (itr_acc != acc_idx.end())
      {
         p_key = graphene::utilities::wif_to_key(key_string);
         from_account = *itr_acc;
         bonus_schedule();
      }
   }
}

int minutes(fc::time_point now) {
    return stoi(string(now).substr(14, 2));
}

int seconds(fc::time_point now) {
    return stoi(string(now).substr(17, 2));
}

string date_string(fc::time_point now) {
    return string(now).substr(0, 10);
}

void application::set_key_from_file(fc::path path)
{
    if( path.is_relative() )
        path = fc::current_path() / path;
    if( fc::exists(path) ) 		{
        fc::read_file_contents( path, key_string );
    }
}

void application::bonus_schedule()
{
    fc::time_point now = fc::time_point::now();
    int wake_every = 60;
    auto min = (minutes(now) + 1) % wake_every;
    fc::time_point next_wakeup( now + fc::minutes(wake_every - min) + fc::seconds(60 - seconds(now)));
    std::cout << "==========" << std::endl << "NEXT WAKEUP: " << string(next_wakeup) << std::endl << "==========" << std::endl;
    fc::schedule([this]{bonus_schedule_loop();},
                 next_wakeup, "Bonus schedule loop");
}

void application::bonus_schedule_loop()
{
    bonus_schedule();
    if (!bonus_storage.empty()) {
       std::cout << "STILL ISSUING" << std::endl;
       return;
    }
    auto& d = *my->_chain_db;
    if (get_core_transfers("Daily mining reward").size()) {
        std::cout << "return1" << std::endl;
        referrer_bonus();
        return;
    }
    bonus_storage.push_back(op_info(from_account, 1000, "Daily mining reward", true));
    auto& idx = d.get_index_type<chain::account_index>();
    const auto asset = my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
    idx.inspect_all_objects( [&d,this,&asset](const chain::object& obj){
        const chain::account_object& account = static_cast<const chain::account_object&>(obj);

        auto balance = d.get_balance(account.id, asset->id).amount;
        if (balance.value == 0) return;
        
        auto bonuses = get_daily_bonus(account.id);
        if (bonuses.size()) return;
        
        uint64_t quantity = 0.0065 * balance.value;
        if (quantity < 1) return;
        try {
            bonus_storage.push_back(op_info(account, quantity, "Daily mining reward"));
        } catch (...) {}
    });
    fc::time_point next_wakeup( fc::time_point::now() + fc::seconds(20));
    fc::schedule([this]{issue_from_storage();},
                next_wakeup, "issue_from_storage");
}

void application::issue_from_storage() {
    const auto SEND_STRANSACTION_EVERY = fc::milliseconds(71); //ms
    if (!bonus_storage.size()) return;

    auto asset = *my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
    auto elem = bonus_storage.back();
    bonus_storage.pop_back();
    if (elem.is_core_transfer) {
        bonus_issued(elem.memo_string);
    } else {
    	issue_bonus(elem.to_account, elem.quantity, elem.memo_string);
    }
    fc::time_point next_wakeup( fc::time_point::now() + SEND_STRANSACTION_EVERY);
    fc::schedule([this]{issue_from_storage();},
                 next_wakeup, "issue_from_storage");
}

void application::referrer_bonus()
{
 auto& d = *my->_chain_db;
 const auto& idx = d.get_index_type<chain::account_index>();
 if (get_core_transfers("Referral reward").size()) {
     std::cout << "return2" << std::endl;
     return;
 }
 bonus_storage.push_back(op_info(from_account, 1000, "Referral reward", true));
 const auto asset = my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
 idx.inspect_all_objects( [&](const chain::object& obj) {
     const chain::account_object& account = static_cast<const chain::account_object&>(obj);

     const uint64_t balance = d.get_balance(account.id, asset->id).amount.value;
     if (balance < 200 * PRECISION) return;
     auto history = get_history(account_id_type(account.id));
     for( auto h = history.begin(); h < history.end(); h++) {
         if (h->op.which() == 14) {
             auto op = h->op.get<asset_issue_operation>();
             std::string message = op.memo->get_message(*p_key, op.memo->to);
             if (message.find("Referral reward") != std::string::npos)  {
                 std::cout << "Referral reward found: " << account.name << std::endl;
                 return;
             }
         }
     }
     auto bonuses = get_daily_bonus(account.id);
     if (!bonuses.size()) return;

     uint64_t bonus_value = 0;
     std::vector<account_object> level_1, level_2, level_3;
     int level_1_partners = 0, level_2_partners = 0, level_3_partners = 0;

     std::tie(level_1, level_1_partners) = get_referrers(account.id);

     if (level_1_partners < 5) return;
     std::map<account_id_type, Unit*> search;
     for (auto& l1: level_1) {
         search.emplace(l1.get_id(), new Unit(l1.get_id(), l1.name, d.get_balance(l1.id, asset->id).amount.value, 1) );
     }
     idx.inspect_all_objects( [&](const chain::object& obj){
         const account_object& ref = static_cast<const chain::account_object&>(obj);

         auto searchIt = search.find(ref.referrer);
         if (searchIt != search.end()) {
             auto balance = d.get_balance(ref.id, asset->id).amount.value;
             if (searchIt->second->level == 1) {
                 level_2.push_back(ref);
                 if (balance >= 100 * PRECISION)
                     level_2_partners++;
             } else {
                 level_3.push_back(ref);
                 if (balance >= 100 * PRECISION)
                     level_3_partners++;
             }
             auto nElem = new Unit(ref.get_id(), ref.name, balance, searchIt->second->level + 1);
             search.emplace(ref.get_id(), nElem);
         }
     });

     // for (auto& ref : level_1) {
     //     std::vector<account_object> refs;
     //     int partners_count = 0;
     //     std::tie(refs, partners_count) = get_referrers(ref.id);
     //     level_2_partners += partners_count;
     //     level_2.insert(level_2.end(), refs.begin(), refs.end());
     // }
     // std::tie(level_3, level_3_partners) = scan_referrers(level_2);
     std::stringstream ref_s;
     ref_s << "[";
     std::string sep = "";
     if (level_2_partners >= 25) {
         if (balance < 500 * PRECISION) return;
         int all_count = level_1_partners + level_2_partners + level_3_partners;
         double bonus_percent = 0;
         if (all_count < 125) {
             bonus_percent = 0.2;
         } else if (all_count < 625) {
             if (balance >= 1000 * PRECISION) bonus_percent = 0.15;
         } else if (all_count < 3125) {
             if (balance >= 2000 * PRECISION) bonus_percent = 0.10;
         } else if (all_count < 15625) {
             if (balance >= 3000 * PRECISION) bonus_percent = 0.05;
         } else if (all_count < 78125) {
             if (balance >= 4000 * PRECISION) bonus_percent = 0.0025;
         } else {
             if (balance >= 5000 * PRECISION) bonus_percent = 0.0025;
         }
         if (bonus_percent == 0) return;
         for (auto& ref : level_1) {
             auto bonuses = get_daily_bonus(ref.id);
             if (bonuses.size()) {
                 int bonus = bonuses[0].asset_to_issue.amount.value * bonus_percent;
                 bonus_value += bonus;
                 ref_s << sep << "{\"i\":" << ref.get_id().instance.value << ",\"l\":1" << ",\"v\":" << bonus << "}";
                 sep = ",";
             }
         }
         for (auto& ref : level_2) {
             auto bonuses = get_daily_bonus(ref.id);
             if (bonuses.size()) {
                 int bonus = bonuses[0].asset_to_issue.amount.value * bonus_percent;
                 bonus_value += bonus;
                 ref_s << sep << "{\"i\":" << ref.get_id().instance.value << ",\"l\":2" << ",\"v\":" << bonus << "}";
             }
         }
         for (auto& ref : level_3) {
             auto bonuses = get_daily_bonus(ref.id);
             if (bonuses.size()) {
                 int bonus = bonuses[0].asset_to_issue.amount.value * bonus_percent;
                 bonus_value += bonus;
                 ref_s << sep << "{\"i\":" << ref.get_id().instance.value << ",\"l\":3" << ",\"v\":" << bonus << "}";
             }
         }
     } else {
         for (auto& ref : level_1) {
             auto bonuses = get_daily_bonus(ref.id);
             if (bonuses.size()) {
                 int bonus = bonuses[0].asset_to_issue.amount.value * 0.25;
                 bonus_value += bonus;
                 ref_s << sep << "{\"i\":" << ref.get_id().instance.value << ",\"l\":1" << ",\"v\":" << bonus << "}";
                 sep = ",";
             }
         }
     }
     ref_s << "]";
     std::cout << "issue bonus " << account.name << " " << bonus_value << " level1: " << level_1.size() << " level2: " << level_2.size() << " level3: " << level_3.size() << std::endl;
     bonus_storage.push_back(op_info(account, bonus_value, "Referral reward " + ref_s.str()));
     // issue_bonus(account, bonus_value, "Referral reward " + ref_s.str());
 });
 fc::time_point next_wakeup( fc::time_point::now() + fc::seconds(20));
 fc::schedule([this]{issue_from_storage();},
             next_wakeup, "issue_from_storage");
}
void application::bonus_issued(std::string with_memo) {
    auto exchange = my->_chain_db->get<account_object>(chain::account_id_type(19));

    chain::transfer_operation op;
    op.from = from_account.id;
    op.to = exchange.id;
    op.amount = asset(1000);
    
    op.memo = memo_data();
    op.memo->from = from_account.options.memo_key;
    op.memo->to = exchange.options.memo_key;
    op.memo->set_message(*p_key,exchange.options.memo_key, with_memo);

    chain::signed_transaction trx;
    trx.operations.push_back(op);
    auto dyn_props = my->_chain_db->get_dynamic_global_properties();
    trx.set_reference_block( dyn_props.head_block_id );
    my->_chain_db->current_fee_schedule().set_fee( trx.operations.back() );
    
    trx.set_expiration( my->_chain_db->get_slot_time( 120 ) );
    trx.sign(*p_key, my->_chain_db->get_chain_id());
    // my->_chain_db->push_transaction(trx);
    my->_p2p_network->broadcast_transaction(trx);
}
    
std::tuple<std::vector<account_object>, int> application::get_referrers(account_id_type account_id)
{
    auto& d = *my->_chain_db;
    const auto& idx = d.get_index_type<chain::account_index>();
    std::vector<account_object> result;
    int count = 0;
    auto asset = my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
    idx.inspect_all_objects( [&d, &account_id,&result,&asset,&count](const chain::object& obj){
        const account_object& account = static_cast<const chain::account_object&>(obj);
        auto balance = d.get_balance(account.id, asset->id).amount.value;
        if (account.id != account_id &&
            account.referrer == account_id) {
            result.push_back(account);
            if (balance >= 100000)
                count++;
        }
    });
    return std::make_tuple(result, count);
}
    
std::vector<asset_issue_operation> application::get_daily_bonus(chain::account_id_type account_id)
{
    auto history = get_history(account_id);
    std::vector<asset_issue_operation> result;
    for( auto h = history.begin(); h < history.end(); h++) {
        if (h->op.which() == 14) {
            auto op = h->op.get<asset_issue_operation>();
            if ("Daily mining reward" == op.memo->get_message(*p_key, op.memo->to)) {
                result.push_back(op);
            };
        }
    }
    return result;
}
    
vector<operation_history_object> application::get_history(account_id_type account)
{
    auto& db = *my->_chain_db;
    vector<operation_history_object> result;
    const auto& stats = account(db).statistics(db);
        
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    
    while(date_string(time_point(db.fetch_block_by_number(node->operation_id.operator()(db).block_num)->timestamp)) == date_string(time_point::now() - fc::hours(9))) {
        result.push_back( node->operation_id(db) );
        if (node->next == account_transaction_history_id_type()) break;
        node = &node->next(db);
    }
    return result;
}
    
vector<transfer_operation> application::get_core_transfers(std::string with_memo)
{
    auto exchange_id = account_id_type(19);
    account_id_type from = from_account.id;
    auto& db = *my->_chain_db;
    vector<transfer_operation> result;
    const auto& stats = from(db).statistics(db);
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    while(date_string(time_point(db.fetch_block_by_number(node->operation_id.operator()(db).block_num)->timestamp)) >= date_string(time_point::now() - fc::hours(9))) {
       auto op_hist = node->operation_id(db);
        if (op_hist.op.which() == 0) {
            auto op = op_hist.op.get<transfer_operation>();
            if (op.memo.valid() && 
                op.from == from &&
                op.to == exchange_id) {
                auto memo_message = op.memo->get_message(*p_key, op.memo->to);
                if (memo_message.substr(0, with_memo.size()) == with_memo) {
                    result.push_back(op);
                }
            }
        }
        if (node->next == account_transaction_history_id_type()) break;
        node = &node->next(db);
    }
    return result;
}

void application::issue_bonus(const chain::account_object& to_account, long long quant, std::string with_memo)
{
    auto asst = *my->_chain_db->get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
    
    chain::signed_transaction trx;
    chain::asset_issue_operation op;
    op.issuer = asst.issuer;
    op.asset_to_issue = asst.amount(quant);
    op.issue_to_account = to_account.id;
    
    op.memo = memo_data();
    op.memo->from = from_account.options.memo_key;
    op.memo->to = to_account.options.memo_key;
    op.memo->set_message(*p_key,to_account.options.memo_key, with_memo);
    trx.operations.push_back(op);
    trx.set_expiration( my->_chain_db->get_slot_time( 120 ) );
    auto dyn_props = my->_chain_db->get_dynamic_global_properties();
    trx.set_reference_block( dyn_props.head_block_id );
    my->_chain_db->current_fee_schedule().set_fee( trx.operations.back() );
    trx.sign(*p_key, my->_chain_db->get_chain_id());
    // my->_chain_db->push_transaction(trx);
    my->_p2p_network->broadcast_transaction(trx);
}

std::shared_ptr<abstract_plugin> application::get_plugin(const string& name) const
{
   return my->_plugins[name];
}

net::node_ptr application::p2p_node()
{
   return my->_p2p_network;
}

std::shared_ptr<chain::database> application::chain_database() const
{
   return my->_chain_db;
}

void application::set_block_production(bool producing_blocks)
{
   my->_is_block_producer = producing_blocks;
}

fc::optional< api_access_info > application::get_api_access_info( const string& username )const
{
   return my->get_api_access_info( username );
}

void application::set_api_access_info(const string& username, api_access_info&& permissions)
{
   my->set_api_access_info(username, std::move(permissions));
}

bool application::is_finished_syncing() const
{
   return my->_is_finished_syncing;
}

void graphene::app::application::add_plugin(const string& name, std::shared_ptr<graphene::app::abstract_plugin> p)
{
   my->_plugins[name] = p;
}

void application::shutdown_plugins()
{
   for( auto& entry : my->_plugins )
      entry.second->plugin_shutdown();
   return;
}
void application::shutdown()
{
   if( my->_p2p_network )
      my->_p2p_network->close();
   if( my->_chain_db )
      my->_chain_db->close();
}

void application::initialize_plugins( const boost::program_options::variables_map& options )
{
   for( auto& entry : my->_plugins )
      entry.second->plugin_initialize( options );
   return;
}

void application::startup_plugins()
{
   for( auto& entry : my->_plugins )
      entry.second->plugin_startup();
   return;
}

} } // namespace graphene::app
