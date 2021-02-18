#include <fc/network/http/websocket.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>

#ifdef HAS_ZLIB
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#else
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#endif

#include <fc/io/json.hpp>
#include <fc/optional.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/asio.hpp>

#if WIN32
#include <wincrypt.h>
#endif

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "rpc"

namespace fc { namespace http {

   namespace detail {
#if WIN32
      // taken from https://stackoverflow.com/questions/39772878/reliable-way-to-get-root-ca-certificates-on-windows/40710806
      static void add_windows_root_certs(boost::asio::ssl::context &ctx)
      {
         HCERTSTORE hStore = CertOpenSystemStore( 0, "ROOT" );
         if( hStore == NULL )
            return;

         X509_STORE *store = X509_STORE_new();
         PCCERT_CONTEXT pContext = NULL;
         while( (pContext = CertEnumCertificatesInStore( hStore, pContext )) != NULL )
         {
            X509 *x509 = d2i_X509( NULL, (const unsigned char **)&pContext->pbCertEncoded,
                                   pContext->cbCertEncoded);
            if( x509 != NULL )
            {
               X509_STORE_add_cert( store, x509 );
               X509_free( x509 );
            }
         }

         CertFreeCertificateContext( pContext );
         CertCloseStore( hStore, 0 );

         SSL_CTX_set_cert_store( ctx.native_handle(), store );
      }
#endif
      struct asio_with_stub_log : public websocketpp::config::asio {

          typedef asio_with_stub_log type;
          typedef asio base;

          typedef base::concurrency_type concurrency_type;

          typedef base::request_type request_type;
          typedef base::response_type response_type;

          typedef base::message_type message_type;
          typedef base::con_msg_manager_type con_msg_manager_type;
          typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

          typedef websocketpp::log::stub elog_type;
          typedef websocketpp::log::stub alog_type;

          typedef base::rng_type rng_type;

          struct transport_config : public base::transport_config {
              typedef type::concurrency_type concurrency_type;
              typedef type::alog_type alog_type;
              typedef type::elog_type elog_type;
              typedef type::request_type request_type;
              typedef type::response_type response_type;
              typedef websocketpp::transport::asio::basic_socket::endpoint socket_type;
          };

          typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

          // permessage_compress extension
          struct permessage_deflate_config {};
#ifdef HAS_ZLIB
          typedef websocketpp::extensions::permessage_deflate::enabled <permessage_deflate_config>
                permessage_deflate_type;
#else
          typedef websocketpp::extensions::permessage_deflate::disabled <permessage_deflate_config>
                permessage_deflate_type;
#endif
      };
      struct asio_tls_with_stub_log : public websocketpp::config::asio_tls {

          typedef asio_with_stub_log type;
          typedef asio_tls base;

          typedef base::concurrency_type concurrency_type;

          typedef base::request_type request_type;
          typedef base::response_type response_type;

          typedef base::message_type message_type;
          typedef base::con_msg_manager_type con_msg_manager_type;
          typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

          typedef websocketpp::log::stub elog_type;
          typedef websocketpp::log::stub alog_type;

          typedef base::rng_type rng_type;

          struct transport_config : public base::transport_config {
              typedef type::concurrency_type concurrency_type;
              typedef type::alog_type alog_type;
              typedef type::elog_type elog_type;
              typedef type::request_type request_type;
              typedef type::response_type response_type;
              typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
          };

          typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

          // permessage_compress extension
          struct permessage_deflate_config {};
#ifdef HAS_ZLIB
          typedef websocketpp::extensions::permessage_deflate::enabled <permessage_deflate_config>
                permessage_deflate_type;
#else
          typedef websocketpp::extensions::permessage_deflate::disabled <permessage_deflate_config>
                permessage_deflate_type;
#endif
      };
      struct asio_tls_stub_log : public websocketpp::config::asio_tls {
         typedef asio_tls_stub_log type;
         typedef asio_tls base;

         typedef base::concurrency_type concurrency_type;

         typedef base::request_type request_type;
         typedef base::response_type response_type;

         typedef base::message_type message_type;
         typedef base::con_msg_manager_type con_msg_manager_type;
         typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

         typedef websocketpp::log::stub elog_type;
         typedef websocketpp::log::stub alog_type;

         typedef base::rng_type rng_type;

         struct transport_config : public base::transport_config {
            typedef type::concurrency_type concurrency_type;
            typedef type::alog_type alog_type;
            typedef type::elog_type elog_type;
            typedef type::request_type request_type;
            typedef type::response_type response_type;
            typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
         };

         typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

         // permessage_compress extension
         struct permessage_deflate_config {};
#ifdef HAS_ZLIB
         typedef websocketpp::extensions::permessage_deflate::enabled <permessage_deflate_config>
               permessage_deflate_type;
#else
         typedef websocketpp::extensions::permessage_deflate::disabled <permessage_deflate_config>
               permessage_deflate_type;
#endif
      };

      template<typename T>
      class websocket_connection_impl : public websocket_connection
      {
         public:
            websocket_connection_impl( T con ) : _ws_connection(con)
            {
               _remote_endpoint = con->get_remote_endpoint();
            }

            virtual ~websocket_connection_impl()
            {
            }

            virtual void send_message( const std::string& message )override
            {
               ilog( "[OUT] ${remote_endpoint} ${msg}",
                     ("remote_endpoint",_remote_endpoint) ("msg",message) );
               auto ec = _ws_connection->send( message );
               FC_ASSERT( !ec, "websocket send failed: ${msg}", ("msg",ec.message() ) );
            }
            virtual void close( int64_t code, const std::string& reason  )override
            {
               _ws_connection->close(code,reason);
            }

            virtual std::string get_request_header(const std::string& key)override
            {
              return _ws_connection->get_request_header(key);
            }

            T _ws_connection;
      };

      template<typename T>
      class possibly_proxied_websocket_connection : public websocket_connection_impl<T>
      {
         public:
            possibly_proxied_websocket_connection( T con, const std::string& forward_header_key )
               : websocket_connection_impl<T>(con)
            {
               // By calling the parent's constructor, _remote_endpoint has been initialized.
               // Now try to extract remote address from the header, if found, overwrite it
               if( !forward_header_key.empty() )
               {
                  const std::string value = this->get_request_header( forward_header_key );
                  if( !value.empty() )
                     this->_remote_endpoint = value;
               }
            }

            virtual ~possibly_proxied_websocket_connection()
            {
            }
      };

      typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

      using websocketpp::connection_hdl;

      template<typename T>
      class generic_websocket_server_impl
      {
         public:
            generic_websocket_server_impl( const std::string& forward_header_key )
               :_server_thread( fc::thread::current() ), _forward_header_key( forward_header_key )
            {
               _server.clear_access_channels( websocketpp::log::alevel::all );
               _server.init_asio( &fc::asio::default_io_service() );
               _server.set_reuse_addr( true );
               _server.set_open_handler( [this]( connection_hdl hdl ){
                  _server_thread.async( [this, hdl](){
                     auto new_con = std::make_shared<possibly_proxied_websocket_connection<
                              typename websocketpp::server<T>::connection_ptr>>( _server.get_con_from_hdl(hdl),
                                                                                 _forward_header_key );
                     _on_connection( _connections[hdl] = new_con );
                  }).wait();
               });
               _server.set_message_handler( [this]( connection_hdl hdl,
                              typename websocketpp::server<T>::message_ptr msg ){
                  _server_thread.async( [this,hdl,msg](){
                     auto current_con = _connections.find(hdl);
                     if( current_con == _connections.end() )
                        return;
                     auto payload = msg->get_payload();
                     std::shared_ptr<websocket_connection> con = current_con->second;
                     wlog( "[IN] ${remote_endpoint} ${msg}",
                           ("remote_endpoint",con->get_remote_endpoint_string()) ("msg",payload) );
                     ++_pending_messages;
                     auto f = fc::async([this,con,payload](){
                        if( _pending_messages )
                           --_pending_messages;
                        con->on_message( payload );
                     });
                     if( _pending_messages > 100 )
                        f.wait(); // Note: this is a bit strange, because it forces the server to process all
                                  //       100 pending messages (assuming this message is the last one) before
                                  //       trying to accept a new message.
                                  //       Ideally the `wait` should be canceled immediately when the number of
                                  //       pending messages falls below 100. That said, wait on the whole queue,
                                  //       but not wait on one message.
                  }).wait();
               });

               _server.set_socket_init_handler( []( websocketpp::connection_hdl hdl,
                              typename websocketpp::server<T>::connection_type::socket_type& s ) {
                  boost::asio::ip::tcp::no_delay option(true);
                  s.lowest_layer().set_option(option);
               } );

               _server.set_http_handler( [this]( connection_hdl hdl ){
                  _server_thread.async( [this,hdl](){
                     auto con = _server.get_con_from_hdl(hdl);
                     auto current_con = std::make_shared<possibly_proxied_websocket_connection<
                              typename websocketpp::server<T>::connection_ptr>>( con, _forward_header_key );
                     _on_connection( current_con );

                     con->defer_http_response(); // Note: this can tie up resources if send_http_response() is not
                                                 //       called quickly enough
                     std::string remote_endpoint = current_con->get_remote_endpoint_string();
                     std::string request_body = con->get_request_body();
                     wlog( "[HTTP-IN] ${remote_endpoint} ${msg}",
                           ("remote_endpoint",remote_endpoint) ("msg",request_body) );

                     fc::async([current_con, request_body, con, remote_endpoint] {
                        fc::http::reply response = current_con->on_http(request_body);
                        ilog( "[HTTP-OUT] ${remote_endpoint} ${status} ${msg}",
                              ("remote_endpoint",remote_endpoint)
                              ("status",response.status)
                              ("msg",response.body_as_string) );
                        con->set_body( std::move( response.body_as_string ) );
                        con->set_status( websocketpp::http::status_code::value(response.status) );
                        con->send_http_response();
                        current_con->closed();
                     }, "call on_http");
                  }).wait();
               });

               _server.set_close_handler( [this]( connection_hdl hdl ){
                  _server_thread.async( [this,hdl](){
                     if( _connections.find(hdl) != _connections.end() )
                     {
                        _connections[hdl]->closed();
                        _connections.erase( hdl );
                        if( _connections.empty() && _all_connections_closed )
                           _all_connections_closed->set_value();
                     }
                     else
                     {
                        wlog( "unknown connection closed" );
                     }
                  }).wait();
               });

               _server.set_fail_handler( [this]( connection_hdl hdl ){
                  _server_thread.async( [this,hdl](){
                     if( _connections.find(hdl) != _connections.end() )
                     {
                        _connections[hdl]->closed();
                        _connections.erase( hdl );
                        if( _connections.empty() && _all_connections_closed )
                           _all_connections_closed->set_value();
                     }
                     else
                     {
                        // if the server is shutting down, assume this hdl is the server socket
                        if( _server_socket_closed )
                           _server_socket_closed->set_value();
                        else
                           wlog( "unknown connection failed" );
                     }
                  }).wait();
               });
            }

            virtual ~generic_websocket_server_impl()
            {
               if( _server.is_listening() )
               {
                  // _server.stop_listening() may trigger the on_fail callback function (the lambda function set by
                  //   _server.set_fail_handler(...) ) for the listening server socket (note: the connection handle
                  //   associated with the server socket is not in our connection map),
                  // the on_fail callback function may fire (async) a new task which may run really late
                  //   and it will try to access the member variables of this server object,
                  // so we need to wait for it before destructing this object.
                  _server_socket_closed = promise<void>::create();
                  _server.stop_listening();
               }

               // Note: since _connections can be modified by lambda functions in set_*_handler, which are running
               //       in other tasks, perhaps we need to wait for them (especially the one in set_open_handler)
               //       being processed. Otherwise `_all_connections_closed.wait()` may hang.
               if( !_connections.empty() )
               {
                  _all_connections_closed = promise<void>::create();

                  auto cpy_con = _connections;
                  websocketpp::lib::error_code ec;
                  for( auto& item : cpy_con )
                     _server.close( item.first, 0, "server exit", ec );

                  _all_connections_closed->wait();
               }

               if( _server_socket_closed )
                  _server_socket_closed->wait();
            }

            typedef std::map<connection_hdl, websocket_connection_ptr, std::owner_less<connection_hdl> > con_map;

            // Note: std::map is not thread-safe nor task-safe, we may need
            //       to use a mutex or similar to avoid concurrent access.
            con_map                  _connections;   ///< Holds accepted connections
            fc::thread&              _server_thread; ///< The thread that runs the server
            websocketpp::server<T>   _server;        ///< The server
            on_connection_handler    _on_connection; ///< A handler to be called when a new connection is accepted
            fc::promise<void>::ptr   _all_connections_closed; ///< Promise to wait for all connections to be closed
            fc::promise<void>::ptr   _server_socket_closed; ///< Promise to wait for the server socket to be closed
            uint32_t                 _pending_messages = 0; ///< Number of messages not processed, for rate limiting
            std::string              _forward_header_key; ///< A header like "X-Forwarded-For" (XFF) with data IP:port
      };

      class websocket_server_impl : public generic_websocket_server_impl<asio_with_stub_log>
      {
         public:
            websocket_server_impl( const std::string& forward_header_key )
            : generic_websocket_server_impl( forward_header_key )
            {}

            virtual ~websocket_server_impl() {}
      };

      class websocket_tls_server_impl : public generic_websocket_server_impl<asio_tls_stub_log>
      {
         public:
            websocket_tls_server_impl( const string& server_pem, const string& ssl_password,
                                       const std::string& forward_header_key )
               : generic_websocket_server_impl( forward_header_key )
            {
               _server.set_tls_init_handler( [server_pem,ssl_password]( websocketpp::connection_hdl hdl ) {
                     context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
                                             boost::asio::ssl::context::tlsv12 );
                     try {
                        ctx->set_options( boost::asio::ssl::context::default_workarounds |
                                          boost::asio::ssl::context::no_sslv2 |
                                          boost::asio::ssl::context::no_sslv3 |
                                          boost::asio::ssl::context::no_tlsv1 |
                                          boost::asio::ssl::context::no_tlsv1_1 |
                                          boost::asio::ssl::context::single_dh_use );
                        ctx->set_password_callback(
                              [ssl_password](std::size_t max_length, boost::asio::ssl::context::password_purpose){
                                    return ssl_password;
                        });
                        ctx->use_certificate_chain_file(server_pem);
                        ctx->use_private_key_file(server_pem, boost::asio::ssl::context::pem);
                     } catch (std::exception& e) {
                        std::cout << e.what() << std::endl;
                     }
                     return ctx;
               });
            }

            virtual ~websocket_tls_server_impl() {}

      };


      template<typename T>
      class generic_websocket_client_impl
      {
         public:
            generic_websocket_client_impl()
            :_client_thread( fc::thread::current() )
            {
                _client.clear_access_channels( websocketpp::log::alevel::all );
                _client.set_message_handler( [this]( connection_hdl hdl,
                                                  typename websocketpp::client<T>::message_ptr msg ){
                   _client_thread.async( [this,msg](){
                        wdump((msg->get_payload()));
                        auto received = msg->get_payload();
                        fc::async( [this,received](){
                           if( _connection )
                               _connection->on_message(received);
                        });
                   }).wait();
                });
                _client.set_close_handler( [this]( connection_hdl hdl ){
                   _client_thread.async( [this](){
                         if( _connection ) {
                            _connection->closed();
                            _connection.reset();
                         }
                      } ).wait();
                   if( _closed )
                      _closed->set_value();
                });
                _client.set_fail_handler( [this]( connection_hdl hdl ){
                   auto con = _client.get_con_from_hdl(hdl);
                   auto message = con->get_ec().message();
                   if( _connection )
                   {
                      _client_thread.async( [this](){
                            if( _connection ) {
                               _connection->closed();
                               _connection.reset();
                            }
                         } ).wait();
                   }
                   if( _connected && !_connected->ready() )
                   {
                       _connected->set_exception( exception_ptr(
                             new FC_EXCEPTION( exception, "${message}", ("message",message)) ) );
                   }
                   if( _closed )
                       _closed->set_value();
                });

                _client.init_asio( &fc::asio::default_io_service() );
            }

            virtual ~generic_websocket_client_impl()
            {
               if( _connection )
               {
                  _connection->close(0, "client closed");
                  _connection.reset();
               }
               if( _closed )
                  _closed->wait();
            }

            websocket_connection_ptr connect( const std::string& uri, const fc::http::headers& headers )
            {
               websocketpp::lib::error_code ec;

               _uri = uri;
               _connected = promise<void>::create("websocket::connect");

               _client.set_open_handler( [this]( websocketpp::connection_hdl hdl ){
                  _hdl = hdl;
                  auto con = _client.get_con_from_hdl(hdl);
                  _connection = std::make_shared<websocket_connection_impl<
                                       typename websocketpp::client<T>::connection_ptr>>( con );
                  _closed = promise<void>::create("websocket::closed");
                  _connected->set_value();
               });

               auto con = _client.get_connection( uri, ec );

               for( const fc::http::header& h : headers )
               {
                  con->append_header( h.key, h.val );
               }

               FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );

               _client.connect(con);
               _connected->wait();
               return _connection;
            }

            fc::promise<void>::ptr             _connected;
            fc::promise<void>::ptr             _closed;
            fc::thread&                        _client_thread;
            websocketpp::client<T>             _client;
            websocket_connection_ptr           _connection;
            std::string                        _uri;
            fc::optional<connection_hdl>       _hdl;
      };

      class websocket_client_impl : public generic_websocket_client_impl<asio_with_stub_log>
      {};

      class websocket_tls_client_impl : public generic_websocket_client_impl<asio_tls_stub_log>
      {
         public:
            websocket_tls_client_impl( const std::string& ca_filename )
            : generic_websocket_client_impl()
            {
                // ca_filename has special values:
                // "_none" disables cert checking (potentially insecure!)
                // "_default" uses default CA's provided by OS

                //
                // We need ca_filename to be copied into the closure, as the
                // referenced object might be destroyed by the caller by the time
                // tls_init_handler() is called.  According to [1], capture-by-value
                // results in the desired behavior (i.e. creation of
                // a copy which is stored in the closure) on standards compliant compilers,
                // but some compilers on some optimization levels
                // are buggy and are not standards compliant in this situation.
                //  Also, keep in mind this is the opinion of a single forum
                // poster and might be wrong.
                //
                // To be safe, the following line explicitly creates a non-reference string
                // which is captured by value, which should have the
                // correct behavior on all compilers.
                //
                // [1] http://www.cplusplus.com/forum/general/142165/
                // [2] http://stackoverflow.com/questions/21443023/capturing-a-reference-by-reference-in-a-c11-lambda
                //

                std::string ca_filename_copy = ca_filename;

                _client.set_tls_init_handler( [this,ca_filename_copy](websocketpp::connection_hdl) {
                   context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
                                           boost::asio::ssl::context::tlsv12);
                   try {
                      ctx->set_options( boost::asio::ssl::context::default_workarounds |
                                        boost::asio::ssl::context::no_sslv2 |
                                        boost::asio::ssl::context::no_sslv3 |
                                        boost::asio::ssl::context::no_tlsv1 |
                                        boost::asio::ssl::context::no_tlsv1_1 |
                                        boost::asio::ssl::context::single_dh_use );

                      setup_peer_verify( ctx, ca_filename_copy );
                   } catch (std::exception& e) {
                      edump((e.what()));
                      std::cout << e.what() << std::endl;
                   }
                   return ctx;
                });

            }
            virtual ~websocket_tls_client_impl() {}

            std::string get_host()const
            {
               return websocketpp::uri( _uri ).get_host();
            }

            void setup_peer_verify( context_ptr& ctx, const std::string& ca_filename )
            {
               if( ca_filename == "_none" )
                  return;
               ctx->set_verify_mode( boost::asio::ssl::verify_peer );
               if( ca_filename == "_default" )
               {
#if WIN32
                  add_windows_root_certs( *ctx );
#else
                  ctx->set_default_verify_paths();
#endif
               }
               else
                  ctx->load_verify_file( ca_filename );
               ctx->set_verify_depth(10);
               ctx->set_verify_callback( boost::asio::ssl::rfc2818_verification( get_host() ) );
            }

      };


   } // namespace detail

   websocket_server::websocket_server( const std::string& forward_header_key )
         :my( new detail::websocket_server_impl( forward_header_key ) ) {}
   websocket_server::~websocket_server(){}

   void websocket_server::on_connection( const on_connection_handler& handler )
   {
      my->_on_connection = handler;
   }

   void websocket_server::listen( uint16_t port )
   {
      my->_server.listen(port);
   }
   void websocket_server::listen( const fc::ip::endpoint& ep )
   {
      my->_server.listen( boost::asio::ip::tcp::endpoint(
            boost::asio::ip::address_v4(uint32_t(ep.get_address())),ep.port()) );
   }

   uint16_t websocket_server::get_listening_port()
   {
      websocketpp::lib::asio::error_code ec;
      return my->_server.get_local_endpoint(ec).port();
   }

   void websocket_server::start_accept() {
      my->_server.start_accept();
   }

   void websocket_server::stop_listening()
   {
      my->_server.stop_listening();
   }

   void websocket_server::close()
   {
      auto cpy_con = my->_connections;
      websocketpp::lib::error_code ec;
      for( auto& connection : cpy_con )
         my->_server.close( connection.first, websocketpp::close::status::normal, "Goodbye", ec );
   }

   websocket_tls_server::websocket_tls_server( const string& server_pem, const string& ssl_password,
                                               const std::string& forward_header_key )
         :my( new detail::websocket_tls_server_impl(server_pem, ssl_password, forward_header_key) )
   {}

   websocket_tls_server::~websocket_tls_server(){}

   void websocket_tls_server::on_connection( const on_connection_handler& handler )
   {
      my->_on_connection = handler;
   }

   void websocket_tls_server::listen( uint16_t port )
   {
      my->_server.listen(port);
   }
   void websocket_tls_server::listen( const fc::ip::endpoint& ep )
   {
      my->_server.listen( boost::asio::ip::tcp::endpoint(
            boost::asio::ip::address_v4(uint32_t(ep.get_address())),ep.port()) );
   }

   uint16_t websocket_tls_server::get_listening_port()
   {
      websocketpp::lib::asio::error_code ec;
      return my->_server.get_local_endpoint(ec).port();
   }

   void websocket_tls_server::start_accept() {
      my->_server.start_accept();
   }

   void websocket_tls_server::stop_listening()
   {
      my->_server.stop_listening();
   }

   void websocket_tls_server::close()
   {
      auto cpy_con = my->_connections;
      websocketpp::lib::error_code ec;
      for( auto& connection : cpy_con )
         my->_server.close( connection.first, websocketpp::close::status::normal, "Goodbye", ec );
   }


   websocket_client::websocket_client( const std::string& ca_filename )
         :my( new detail::websocket_client_impl() ),
          smy(new detail::websocket_tls_client_impl( ca_filename ))
   {}

   websocket_client::~websocket_client(){ }

   websocket_connection_ptr websocket_client::connect( const std::string& uri )
   { try {

      FC_ASSERT( uri.substr(0,4) == "wss:" || uri.substr(0,3) == "ws:", "Unsupported protocol" );

      // WSS
      if( uri.substr(0,4) == "wss:" )
         return smy->connect( uri, _headers );

      // WS
      return my->connect( uri, _headers );

   } FC_CAPTURE_AND_RETHROW( (uri) ) }

   websocket_connection_ptr websocket_client::secure_connect( const std::string& uri )
   {
      return connect( uri );
   }

   void websocket_client::close()
   {
      if( my->_hdl )
         my->_client.close( *my->_hdl, websocketpp::close::status::normal, "Goodbye" );
   }

   void websocket_client::synchronous_close()
   {
      close();
      if( my->_closed )
         my->_closed->wait();
   }

   void websocket_client::append_header(const std::string& key, const std::string& value)
   {
      _headers.emplace_back( key, value );
   }

} } // fc::http
