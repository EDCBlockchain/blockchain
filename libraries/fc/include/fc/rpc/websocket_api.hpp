#pragma once
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/state.hpp>

namespace fc { namespace rpc {

   class websocket_api_connection : public api_connection
   {
      public:
         websocket_api_connection( const std::shared_ptr<fc::http::websocket_connection> &c,
                                   uint32_t max_conversion_depth );
         ~websocket_api_connection();

         virtual variant send_call(
            api_id_type api_id,
            string method_name,
            variants args = variants() ) override;
         virtual variant send_callback(
            uint64_t callback_id,
            variants args = variants() ) override;
         virtual void send_notice(
            uint64_t callback_id,
            variants args = variants() ) override;

      protected:
         response on_message( const std::string& message );
         response on_request( const variant& message );
         void     on_response( const variant& message );

         std::shared_ptr<fc::http::websocket_connection>  _connection;
         fc::rpc::state                                   _rpc_state;
   };

} } // namespace fc::rpc
