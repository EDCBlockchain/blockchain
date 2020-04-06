#pragma once
#include <memory>
#include <string>

namespace fc {
   class appender;
   class log_message;
   class variant;

   class appender_factory {
      public:
       typedef std::shared_ptr<appender_factory> ptr;

       virtual ~appender_factory(){};
       virtual std::shared_ptr<appender> create( const variant& args ) = 0;
   };

   namespace detail {
      template<typename T>
      class appender_factory_impl : public appender_factory {
        public:
           virtual std::shared_ptr<appender> create( const variant& args ) {
              return std::shared_ptr<appender>(new T(args));
           }
      };
   }

   class appender {
      public:
         typedef std::shared_ptr<appender> ptr;

         template<typename T>
         static bool register_appender(const std::string& type) {
            return register_appender( type, appender_factory::ptr( new detail::appender_factory_impl<T>() ) );
         }

         static appender::ptr create( const std::string& name, const std::string& type, const variant& args  );
         static appender::ptr get( const std::string& name );
         static bool          register_appender( const std::string& type, const appender_factory::ptr& f );

         virtual void log( const log_message& m ) = 0;
   };
}
