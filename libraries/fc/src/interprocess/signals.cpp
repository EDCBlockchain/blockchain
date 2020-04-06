#include <fc/asio.hpp>
#include <boost/asio/signal_set.hpp>

namespace fc
{
   std::shared_ptr<boost::asio::signal_set> set_signal_handler( std::function<void(int)> handler, int signal_num )
   {
      std::shared_ptr<boost::asio::signal_set> sig_set( new boost::asio::signal_set( fc::asio::default_io_service(),
                                                                                     signal_num) );
      sig_set->async_wait( [sig_set,handler]( const boost::system::error_code& err, int num )
      {
         if( err != boost::asio::error::operation_aborted )
            handler( num );
         sig_set->cancel();
      } );
      return sig_set;
   }
}
