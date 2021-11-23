#include <chrono>

#pragma once

namespace graphene { namespace utilities {

   std::string format_duration(std::chrono::milliseconds ms)
   {
      using namespace std::chrono;
      auto secs = duration_cast<seconds>(ms);
      ms -= duration_cast<milliseconds>(secs);
      auto mins = duration_cast<minutes>(secs);
      secs -= duration_cast<seconds>(mins);
      auto hour = duration_cast<hours>(mins);
      mins -= duration_cast<minutes>(hour);

      std::stringstream ss;
      ss << hour.count() << "h:" << mins.count() << "m:" << secs.count() << "s:" << ms.count() << "ms";
      return ss.str();
   }

} } //graphene::utilities

