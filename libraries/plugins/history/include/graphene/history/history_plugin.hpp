// see LICENSE.txt

#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/operation_history_object.hpp>

#include <fc/thread/future.hpp>

namespace graphene { namespace history {
   using namespace chain;
   //using namespace graphene::db;
   //using boost::multi_index_container;
   //using namespace boost::multi_index;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef HISTORY_SPACE_ID
#define HISTORY_SPACE_ID 5
#endif

enum account_history_object_type
{
   key_account_object_type = 0,
   bucket_object_type = 1 ///< used in market_history_plugin
};

namespace detail
{
   class history_plugin_impl;
}

class history_plugin : public graphene::app::plugin
{
   public:
      history_plugin();
      virtual ~history_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      flat_set<account_id_type> tracked_accounts()const;

      friend class detail::history_plugin_impl;
      std::unique_ptr<detail::history_plugin_impl> my;
};

} } //graphene::history
