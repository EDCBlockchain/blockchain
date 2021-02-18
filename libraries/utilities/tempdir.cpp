// see LICENSE.txt

#include <graphene/utilities/tempdir.hpp>

#include <cstdlib>

namespace graphene { namespace utilities {

fc::path temp_directory_path()
{
   const char* graphene_tempdir = getenv("GRAPHENE_TEMPDIR");
   if( graphene_tempdir != nullptr )
      return fc::path( graphene_tempdir );
   return fc::temp_directory_path() / "graphene-tmp";
}

} } // graphene::utilities
