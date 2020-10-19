#include <fc/network/http/websocket.hpp>
#include <fc/asio.hpp>

#include <iostream>
#include <chrono>
#include <fc/log/logger.hpp>
#include <fc/log/console_appender.hpp>

int main(int argc, char** argv)
{
   // set up logging
   fc::appender::ptr ca(new fc::console_appender);
   fc::logger l = fc::logger::get("rpc");
   l.add_appender( ca );
   
   fc::http::websocket_server server("MyForwardHeaderKey");

   server.on_connection([&]( const fc::http::websocket_connection_ptr& c ){
       c->on_message_handler([&](const std::string& s){
            c->send_message("echo: " + s);
          });
   });

   server.listen( 0 );
   server.start_accept();

   wlog( "Port: ${port}",  ("port", server.get_listening_port()) );

   while( true )
   {
       fc::usleep( fc::microseconds(100) );
   }
}
