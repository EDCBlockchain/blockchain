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

#include <iostream>

#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/wallet/wallet.hpp>

#include <boost/program_options.hpp>

#include <fc/interprocess/signals.hpp>
#include <fc/asio.hpp>

#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#ifdef WIN32
# include <signal.h>
#else
# include <csignal>
#endif

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace graphene::wallet;
using namespace std;
namespace bpo = boost::program_options;

fc::log_level string_to_level(string level)
{
   fc::log_level result;
   if(level == "info")
      result = fc::log_level::info;
   else if(level == "debug")
      result = fc::log_level::debug;
   else if(level == "warn")
      result = fc::log_level::warn;
   else if(level == "error")
      result = fc::log_level::error;
   else if(level == "all")
      result = fc::log_level::all;
   else
      FC_THROW("Log level not allowed. Allowed levels are info, debug, warn, error and all.");

   return result;
}

void setup_logging(string console_level, bool file_logger, string file_level, string file_name)
{
   fc::logging_config cfg;

   // console logger
   fc::console_appender::config console_appender_config;
   console_appender_config.level_colors.emplace_back(
   fc::console_appender::level_color(fc::log_level::debug,
                                     fc::console_appender::color::green));
   console_appender_config.level_colors.emplace_back(
   fc::console_appender::level_color(fc::log_level::warn,
                                     fc::console_appender::color::brown));
   console_appender_config.level_colors.emplace_back(
   fc::console_appender::level_color(fc::log_level::error,
                                     fc::console_appender::color::red));
   cfg.appenders.push_back(fc::appender_config( "default", "console", fc::variant(console_appender_config, 20)));
   cfg.loggers = { fc::logger_config("default"), fc::logger_config( "rpc") };
   cfg.loggers.front().level = string_to_level(console_level);
   cfg.loggers.front().appenders = {"default"};

   // file logger
   if(file_logger) {
      fc::path data_dir;
      fc::path log_dir = data_dir / "cli_wallet_logs";
      fc::file_appender::config ac;
      ac.filename             = log_dir / file_name;
      ac.flush                = true;
      ac.rotate               = true;
      ac.rotation_interval    = fc::hours( 1 );
      ac.rotation_limit       = fc::days( 1 );
      cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac, 5)));
      cfg.loggers.back().level = string_to_level(file_level);
      cfg.loggers.back().appenders = {"rpc"};
      fc::configure_logging( cfg );
      ilog ("Logging RPC to file: " + (ac.filename).preferred_string());
   }
}

int main( int argc, char** argv )
{

   try {

      boost::program_options::options_description opts;
         opts.add_options()
         ("help,h", "Print this help message and exit.")
         ("server-rpc-endpoint,s", bpo::value<string>()->implicit_value("wss://127.0.0.1:5909"), "Server websocket RPC endpoint")
         ("server-rpc-user,u", bpo::value<string>(), "Server Username")
         ("server-rpc-password,p", bpo::value<string>(), "Server Password")
         ("rpc-endpoint,r", bpo::value<string>()->implicit_value("127.0.0.1:8081"), "Endpoint for wallet websocket RPC to listen on")
         ("rpc-tls-endpoint,t", bpo::value<string>()->implicit_value("127.0.0.1:8092"), "Endpoint for wallet websocket TLS RPC to listen on")
         ("rpc-tls-certificate,c", bpo::value<string>()->implicit_value("server.pem"), "PEM certificate for wallet websocket TLS RPC")
         ("rpc-http-endpoint,H", bpo::value<string>()->implicit_value("127.0.0.1:8082"), "Endpoint for wallet HTTP RPC to listen on")
         ("daemon,d", "Run the wallet in daemon mode" )
         ("wallet-file,w", bpo::value<string>()->implicit_value("wallet.json"), "wallet to load")
         ("chain-id", bpo::value<string>(), "chain ID to connect to")
         ("delayed",  bpo::bool_switch(), "Connect to delayed node if specified")
         ("no-backups",  bpo::bool_switch(), "Disable before/after import key backups creation if specified")
         ("io-threads", bpo::value<uint16_t>()->implicit_value(1), "Number of IO threads, default to 1")
         ("logs-rpc-console-level", bpo::value<string>()->default_value("info"), "Level of console logging")
         ("logs-rpc-file", bpo::value<bool>()->default_value(true), "Turn on/off file logging")
         ("logs-rpc-file-level", bpo::value<string>()->default_value("debug"), "Level of file logging")
         ("logs-rpc-file-name", bpo::value<string>()->default_value("rpc.log"), "File name for file rpc logs")
         ;

      bpo::variables_map options;

      bpo::store( bpo::parse_command_line(argc, argv, opts), options );

      if( options.count("help") )
      {
         std::cout << opts << "\n";
         return 0;
      }

      if ( options.count("io-threads") )
      {
         const uint16_t num_threads = options["io-threads"].as<uint16_t>();
         fc::asio::default_io_service_scope::set_num_threads(num_threads);
      }

      // logging
      setup_logging(options.at("logs-rpc-console-level").as<string>(),options.at("logs-rpc-file").as<bool>(),
                    options.at("logs-rpc-file-level").as<string>(), options.at("logs-rpc-file-name").as<string>());

      fc::ecc::private_key committee_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));

      idump( (key_to_wif( committee_private_key ) ) );

      fc::ecc::private_key nathan_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      public_key_type nathan_pub_key = nathan_private_key.get_public_key();
      idump( (nathan_pub_key) );
      idump( (key_to_wif( nathan_private_key ) ) );

      //
      // TODO:  We read wallet_data twice, once in main() to grab the
      //    socket info, again in wallet_api when we do
      //    load_wallet_file().  Seems like this could be better
      //    designed.
      //

      wallet_data wdata;

      fc::path wallet_file( options.count("wallet-file") ? options.at("wallet-file").as<string>() : "wallet.json");
      if( fc::exists( wallet_file ) )
      {
         wdata = fc::json::from_file( wallet_file ).as<wallet_data>(GRAPHENE_MAX_NESTED_OBJECTS);
         if( options.count("chain-id") )
         {
            // the --chain-id on the CLI must match the chain ID embedded in the wallet file
            if( chain_id_type(options.at("chain-id").as<std::string>()) != wdata.chain_id )
            {
               std::cout << "Chain ID in wallet file does not match specified chain ID\n";
               return 1;
            }
         }
      }
      else
      {
         if( options.count("chain-id") )
         {
            wdata.chain_id = chain_id_type(options.at("chain-id").as<std::string>());
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (from CLI)\n";
         }
         else
         {
            wdata.chain_id = chain_id_type("979b29912e5546dbf47604692aafc94519f486c56221a5705f0c7f5f294df126");
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (from egenesis)\n";
         }
      }

      // but allow CLI to override
      if (options.count("delayed") && options.at("delayed").as<bool>())
         wdata.ws_server = "wss://127.0.0.1:5909";
      else if (options.count("server-rpc-endpoint"))
         wdata.ws_server = options.at("server-rpc-endpoint").as<std::string>();

      if( options.count("server-rpc-user") )
         wdata.ws_user = options.at("server-rpc-user").as<std::string>();
      if( options.count("server-rpc-password") )
         wdata.ws_password = options.at("server-rpc-password").as<std::string>();

      fc::http::websocket_client client;
      idump((wdata.ws_server));
      auto con  = client.connect( wdata.ws_server );
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(con, GRAPHENE_MAX_NESTED_OBJECTS);

      auto remote_api = apic->get_remote_api< login_api >(1);
      edump((wdata.ws_user)(wdata.ws_password) );
      FC_ASSERT( remote_api->login( wdata.ws_user, wdata.ws_password ), "Failed to log in to API server" );

      auto wapiptr = std::make_shared<wallet_api>( wdata, remote_api );

      if (wdata.ws_user.length() > 1) {
         wapiptr->set_secure_api(remote_api->secure());
      }

      wapiptr->set_wallet_filename( wallet_file.generic_string() );
      wapiptr->load_wallet_file();
      if (options.count("no-backups") && options.at("no-backups").as<bool>()) {
         wapiptr->disable_backups();
      }

      fc::api<wallet_api> wapi(wapiptr);

      auto wallet_cli = std::make_shared<fc::rpc::cli>(GRAPHENE_MAX_NESTED_OBJECTS);
      for( auto& name_formatter : wapiptr->get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

      boost::signals2::scoped_connection closed_connection(con->closed.connect([wallet_cli]{
         cerr << "Server has disconnected us.\n";
         wallet_cli->stop();
      }));

      if( wapiptr->is_new() )
      {
         std::cout << "Please use the set_password method to initialize a new wallet before continuing\n";
         wallet_cli->set_prompt( "new >>> " );
      } else
         wallet_cli->set_prompt( "locked >>> " );

      boost::signals2::scoped_connection locked_connection(wapiptr->lock_changed.connect([&](bool locked) {
         wallet_cli->set_prompt(  locked ? "locked >>> " : "unlocked >>> " );
      }));

      std::shared_ptr<fc::http::websocket_server> _websocket_server;
      if( options.count("rpc-endpoint") )
      {
         _websocket_server = std::make_shared<fc::http::websocket_server>();
         _websocket_server->on_connection([&wapi]( const fc::http::websocket_connection_ptr& c ){
            auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(c, GRAPHENE_MAX_NESTED_OBJECTS);
            wsc->register_api(wapi);
            c->set_session_data( wsc );
         });
         ilog( "Listening for incoming HTTP and WS RPC requests on ${p}", ("p", options.at("rpc-endpoint").as<string>() ));
         _websocket_server->listen( fc::ip::endpoint::from_string(options.at("rpc-endpoint").as<string>()) );
         _websocket_server->start_accept();
      }

      string cert_pem = "server.pem";
      if( options.count( "rpc-tls-certificate" ) )
         cert_pem = options.at("rpc-tls-certificate").as<string>();

      auto _websocket_tls_server = std::make_shared<fc::http::websocket_tls_server>(cert_pem);
      if( options.count("rpc-tls-endpoint") )
      {
         _websocket_tls_server->on_connection([&wapi]( const fc::http::websocket_connection_ptr& c ){
            auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(c, GRAPHENE_MAX_NESTED_OBJECTS);
            wsc->register_api(wapi);
            c->set_session_data( wsc );
         });
         ilog( "Listening for incoming TLS RPC requests on ${p}", ("p", options.at("rpc-tls-endpoint").as<string>() ));
         _websocket_tls_server->listen( fc::ip::endpoint::from_string(options.at("rpc-tls-endpoint").as<string>()) );
         _websocket_tls_server->start_accept();
      }

      if( !options.count( "daemon" ) )
      {
         auto wallet_cli = std::make_shared<fc::rpc::cli>( GRAPHENE_MAX_NESTED_OBJECTS );

         wallet_cli->set_regex_secret("\\s*(unlock|set_password)\\s*");

         for( auto& name_formatter : wapiptr->get_result_formatters() )
            wallet_cli->format_result( name_formatter.first, name_formatter.second );

         std::cout << "\nType \"help\" for a list of available commands.\n";
         std::cout << "Type \"gethelp <command>\" for info about individual commands.\n\n";
         if( wapiptr->is_new() )
         {
            std::cout << "Please use the \"set_password\" method to initialize a new wallet before continuing\n";
            wallet_cli->set_prompt( "new >>> " );
         }
         else
            wallet_cli->set_prompt( "locked >>> " );

         boost::signals2::scoped_connection locked_connection( wapiptr->lock_changed.connect(
         [wallet_cli](bool locked) {
            wallet_cli->set_prompt( locked ? "locked >>> " : "unlocked >>> " );
         }));

         auto sig_set = fc::set_signal_handler( [wallet_cli](int signal) {
            ilog( "Captured SIGINT not in daemon mode, exiting" );
            fc::set_signal_handler( [](int sig) {}, SIGINT ); // reinstall an empty SIGINT handler
            wallet_cli->cancel();
         }, SIGINT );

         fc::set_signal_handler( [wallet_cli,sig_set](int signal) {
            ilog( "Captured SIGTERM not in daemon mode, exiting" );
            sig_set->cancel();
            fc::set_signal_handler( [](int sig) {}, SIGINT ); // reinstall an empty SIGINT handler
            wallet_cli->cancel();
         }, SIGTERM );
#ifdef SIGQUIT
         fc::set_signal_handler( [wallet_cli,sig_set](int signal) {
            ilog( "Captured SIGQUIT not in daemon mode, exiting" );
            sig_set->cancel();
            fc::set_signal_handler( [](int sig) {}, SIGINT ); // reinstall an empty SIGINT handler
            wallet_cli->cancel();
         }, SIGQUIT );
#endif
         boost::signals2::scoped_connection closed_connection( con->closed.connect( [wallet_cli,sig_set] {
            elog( "Server has disconnected us." );
            sig_set->cancel();
            fc::set_signal_handler( [](int sig) {}, SIGINT ); // reinstall an empty SIGINT handler
            wallet_cli->cancel();
         }));

         wallet_cli->register_api( wapi );
         wallet_cli->start();
         wallet_cli->wait();

         locked_connection.disconnect();
         closed_connection.disconnect();
      }
      else
      {
        fc::promise<int>::ptr exit_promise = fc::promise<int>::create("UNIX Signal Handler");
        fc::set_signal_handler([&exit_promise](int signal) {
           exit_promise->set_value(signal);
        }, SIGINT);

        ilog( "Entering Daemon Mode, ^C to exit" );
        exit_promise->wait();
      }

      wapi->save_wallet_file(wallet_file.generic_string());
      locked_connection.disconnect();
      closed_connection.disconnect();


   }
   catch ( const fc::exception& e )
   {
      std::cout << "Exception: " << e.to_detail_string() << "\n";
      return -1;
   }

   return 0;
}
