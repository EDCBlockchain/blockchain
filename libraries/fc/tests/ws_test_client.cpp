#include <fc/network/http/websocket.hpp>
#include <websocketpp/error.hpp>

#include <iostream>
#include <string>
#include <fc/log/logger.hpp>
#include <fc/log/console_appender.hpp>

int main(int argc, char** argv)
{ 
   try
   {
      // set up logging
      fc::appender::ptr ca(new fc::console_appender);
      fc::logger l = fc::logger::get("rpc");
      l.add_appender( ca );
      
      fc::http::websocket_client client;
      fc::http::websocket_connection_ptr s_conn, c_conn;
      std::string url = argv[1];
      wlog( "Connecting to server at url ${url}", ("url", url) );
      c_conn = client.connect( "ws://" + url );

      std::string echo;
      c_conn->on_message_handler([&](const std::string& s){
               echo = s;
            });
      c_conn->send_message( "hello world" );
      fc::usleep( fc::milliseconds(500) );
      if (echo != std::string("echo: hello world") )
         wlog( "Test1 failed, echo value: [${echo}]", ("echo", echo) );
      c_conn->send_message( "again" );
      fc::usleep( fc::milliseconds(500) );
      if ("echo: again" != echo)
         wlog( "Test2 failed, echo value: [${echo}]", ("echo", echo) );
   }
   catch (const websocketpp::exception& ex)
   {
      elog( "websocketpp::exception thrown: ${err}", ("err", ex.what()) );
   }
}
