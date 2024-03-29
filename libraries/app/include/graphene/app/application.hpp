// see LICENSE.txt

#pragma once

#include <graphene/app/api_access.hpp>
#include <graphene/net/node.hpp>
#include <graphene/chain/database.hpp>

#include <boost/program_options.hpp>

namespace graphene { namespace app {
   namespace detail { class application_impl; }
   using std::string;

   class abstract_plugin;
   class op_info {
      public:
      op_info(chain::account_object obj, uint64_t quantity, std::string memo, bool is_transfer = false) {
            this->to_account = obj;
            this->quantity = quantity;
            this->memo_string = memo;
            this->is_core_transfer = is_transfer;
      }
      chain::account_object to_account;
      uint64_t quantity;
      std::string memo_string;
      bool is_core_transfer;
   };
   class application
   {
      public:
         application();
         ~application();

         void set_program_options( boost::program_options::options_description& command_line_options,
                                   boost::program_options::options_description& configuration_file_options )const;
         void initialize(const fc::path& data_dir, std::shared_ptr<boost::program_options::variables_map> options) const;
         void startup();

         template<typename PluginType>
         std::shared_ptr<PluginType> register_plugin()
         {
            auto plug = std::make_shared<PluginType>();
            plug->plugin_set_app(this);

            boost::program_options::options_description plugin_cli_options("Options for plugin " + plug->plugin_name()), plugin_cfg_options;
            plug->plugin_set_program_options(plugin_cli_options, plugin_cfg_options);
            if( !plugin_cli_options.options().empty() )
               _cli_options.add(plugin_cli_options);
            if( !plugin_cfg_options.options().empty() )
               _cfg_options.add(plugin_cfg_options);

            add_available_plugin(plug);

            return plug;
         }
         std::shared_ptr<abstract_plugin> get_plugin( const string& name )const;

         template<typename PluginType>
         std::shared_ptr<PluginType> get_plugin( const string& name ) const
         {
            std::shared_ptr<abstract_plugin> abs_plugin = get_plugin( name );
            std::shared_ptr<PluginType> result = std::dynamic_pointer_cast<PluginType>( abs_plugin );
            FC_ASSERT( result != std::shared_ptr<PluginType>() );
            return result;
         }

         net::node_ptr                    p2p_node();
         std::shared_ptr<chain::database> chain_database()const;
         void set_block_production(bool producing_blocks);
         fc::optional< api_access_info > get_api_access_info( const string& username )const;
         void set_api_access_info(const string& username, api_access_info&& permissions);

         bool is_finished_syncing()const;
         /// Emitted when syncing finishes (is_finished_syncing will return true)
         boost::signals2::signal<void()> syncing_finished;

         void enable_plugin(const string& name) const;
         bool is_plugin_enabled(const string& name) const;

      private:
         /// Add an available plugin
         void add_available_plugin( std::shared_ptr<abstract_plugin> p ) const;

         std::shared_ptr<detail::application_impl> my;
       
         fc::optional<fc::ecc::private_key> p_key;
         std::vector<op_info> bonus_storage;
         chain::account_object from_account;
       
         std::vector<chain::operation_history_object> get_history(chain::account_id_type account);
         std::tuple<std::vector<chain::account_object>,int> get_referrers(chain::account_id_type);
      //    std::tuple<std::vector<chain::account_object>,int> scan_referrers(std::vector<chain::account_object> level_2);
         std::vector<chain::asset_issue_operation> get_daily_bonus(chain::account_id_type);
         boost::program_options::options_description _cli_options;
         boost::program_options::options_description _cfg_options;
   };

} }
