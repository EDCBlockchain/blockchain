#pragma once
#include <fc/variant.hpp>
#include <functional>
#include <fc/thread/future.hpp>

namespace fc { namespace rpc {
   struct request
   {
      optional<variant>   id;
      std::string         method;
      variants            params;
      optional<std::string> jsonrpc;
   };

   struct error_object
   {
      int64_t           code;
      std::string       message;
      optional<variant> data;
   };

   struct response
   {
      response() {}
      response( const optional<variant>& _id, const variant& _result,
                const optional<string>& version = optional<string>() )
         : id(_id), jsonrpc(version), result(_result) {}
      response( const optional<variant>& _id, const error_object& error,
                const optional<string>& version = optional<string>() )
         : id(_id), jsonrpc(version), error(error) {}
      optional<variant>      id;
      optional<std::string>  jsonrpc;
      optional<fc::variant>  result;
      optional<error_object> error;
   };

   class state
   {
      public:
         typedef std::function<variant(const variants&)>       method;
         ~state();

         void add_method( const std::string& name, method m );
         void remove_method( const std::string& name );

         variant local_call( const string& method_name, const variants& args );
         void    handle_reply( const response& response );

         request start_remote_call( const string& method_name, variants args );
         variant wait_for_response( const variant& request_id );

         void close();

         void on_unhandled( const std::function<variant(const string&,const variants&)>& unhandled );

      private:
         uint64_t                                                   _next_id = 1;
         std::map<variant, fc::promise<variant>::ptr>               _awaiting;
         std::unordered_map<std::string, method>                    _methods;
         std::function<variant(const string&,const variants&)>      _unhandled;
   };
} }  // namespace  fc::rpc

FC_REFLECT( fc::rpc::request, (id)(method)(params)(jsonrpc) );
FC_REFLECT( fc::rpc::error_object, (code)(message)(data) )
FC_REFLECT( fc::rpc::response, (id)(jsonrpc)(result)(error) )
