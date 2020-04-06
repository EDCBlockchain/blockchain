#pragma once
#include <functional>
#include <boost/asio/signal_set.hpp>

namespace fc 
{
   /// Set a handler to process an IPC (inter process communication) signal.
   /// Handler will be called from ASIO thread.
   /// @return shared pointer to the signal_set that holds the handler
   std::shared_ptr<boost::asio::signal_set> set_signal_handler( std::function<void(int)> handler, int signal_num );
}
