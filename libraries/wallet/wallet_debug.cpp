#include "wallet_api_impl.hpp"

namespace graphene { namespace wallet { namespace detail {

    void wallet_api_impl::dbg_make_uia(string creator, string symbol)
   {
//      asset_options opts;
//      opts.flags &= ~(white_list | disable_force_settle | global_settle);
//      opts.issuer_permissions = opts.flags;
//      opts.core_exchange_rate = price(asset(1), asset(1,asset_id_type(1)));
//      create_asset(get_account(creator).name, symbol, 2, opts, {}, true);
   }

   void wallet_api_impl::dbg_make_mia(string creator, string symbol)
   {
//      asset_options opts;
//      opts.flags &= ~white_list;
//      opts.issuer_permissions = opts.flags;
//      opts.core_exchange_rate = price(asset(1), asset(1,asset_id_type(1)));
//      bitasset_options bopts;
//      create_asset(get_account(creator).name, symbol, 2, opts, bopts, true);
   }

   void wallet_api_impl::dbg_push_blocks( const std::string& src_filename, uint32_t count )
   {
      use_debug_api();
      (*_remote_debug)->debug_push_blocks( src_filename, count );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void wallet_api_impl::dbg_generate_blocks( const std::string& debug_wif_key, uint32_t count )
   {
      use_debug_api();
      (*_remote_debug)->debug_generate_blocks( debug_wif_key, count );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void wallet_api_impl::dbg_stream_json_objects( const std::string& filename )
   {
      use_debug_api();
      (*_remote_debug)->debug_stream_json_objects( filename );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void wallet_api_impl::dbg_update_object( const fc::variant_object& update )
   {
      use_debug_api();
      (*_remote_debug)->debug_update_object( update );
      (*_remote_debug)->debug_stream_json_objects_flush();
   }

   void wallet_api_impl::use_debug_api()
   {
      if (_remote_debug) {
         return;
      }
      try {
        _remote_debug = _remote_api->debug();
      }
      catch (const fc::exception& e)
      {
         std::cerr << "\nCouldn't get debug node API.  You probably are not configured\n"
         "to access the debug API on the node you are connecting to.\n"
         "\n"
         "To fix this problem:\n"
         "- Please ensure you are running debug_node, not witness_node.\n"
         "- Please follow the instructions in README.md to set up an apiaccess file.\n"
         "\n";
      }
   }
    
}}} // namespace graphene::wallet::detail