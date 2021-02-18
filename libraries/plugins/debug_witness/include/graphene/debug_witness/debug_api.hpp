// see LICENSE.txt

#pragma once

#include <memory>
#include <string>

#include <fc/api.hpp>
#include <fc/variant_object.hpp>

namespace graphene { namespace app {
class application;
} }

namespace graphene { namespace debug_witness {

namespace detail {
class debug_api_impl;
}

class debug_api
{
   public:
      debug_api( graphene::app::application& app );

      /**
       * Push blocks from existing database.
       */
      void debug_push_blocks( std::string src_filename, uint32_t count );

      /**
       * Generate blocks locally.
       */
      void debug_generate_blocks( std::string debug_key, uint32_t count );

      /**
       * Directly manipulate database objects (will undo and re-apply last block with new changes post-applied).
       */
      void debug_update_object( fc::variant_object update );

      /**
       * Start a node with given initial path.
       */
      // not implemented
      //void start_node( std::string name, std::string initial_db_path );

      /**
       * Save the database to disk.
       */
      // not implemented
      //void save_db( std::string db_path );

      /**
       * Stream objects to file.  (Hint:  Create with mkfifo and pipe it to a script)
       */

      void debug_stream_json_objects( std::string filename );

      /**
       * Flush streaming file.
       */
      void debug_stream_json_objects_flush();

      std::shared_ptr< detail::debug_api_impl > my;
};

} }

FC_API(graphene::debug_witness::debug_api,
       (debug_push_blocks)
       (debug_generate_blocks)
       (debug_update_object)
       (debug_stream_json_objects)
       (debug_stream_json_objects_flush)
     )
