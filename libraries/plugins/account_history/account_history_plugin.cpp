/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/account_history/account_history_plugin.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

namespace graphene { namespace account_history {

namespace detail
{

class account_history_plugin_impl
{
   public:
      account_history_plugin_impl(account_history_plugin& _plugin)
         : _self( _plugin ) { }
      virtual ~account_history_plugin_impl();

      /**
       * This method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void update_account_histories(const signed_block& b);

      graphene::chain::database& database() {
         return _self.database();
      }

      account_history_plugin& _self;
      flat_set<account_id_type> _tracked_accounts;
};

account_history_plugin_impl::~account_history_plugin_impl() {
   return;
}

void account_history_plugin_impl::update_account_histories(const signed_block& b)
{
   graphene::chain::database& db = database();
   const vector<optional<operation_history_object>>& hist = db.get_applied_operations();

   for (const optional<operation_history_object>& o_op: hist)
   {
      // add to the operation history index
      const auto& oho = db.create<operation_history_object>([&](operation_history_object& h)
      {
         if (o_op.valid())
         {
            h.op = (*o_op).op;
            h.result = (*o_op).result;
            h.block_num = (*o_op).block_num;
            h.trx_in_block = (*o_op).trx_in_block;
            h.op_in_trx = (*o_op).op_in_trx;
            h.virtual_op = (*o_op).virtual_op;
            h.block_time = b.timestamp;
         }
      });

      if (!o_op.valid())
      {
         ilog( "removing failed operation with ID: ${id}", ("id", oho.id) );
         db.remove( oho );
         continue;
      }

      const operation_history_object& op = *o_op;

      // get the set of accounts this operation applies to
      flat_set<account_id_type> impacted_acc;
      flat_set<fund_id_type> impacted_funds_tmp;

      vector<authority> other;
      operation_get_required_authorities(op.op, impacted_acc, impacted_acc, other);

      if (op.op.which() == operation::tag<account_create_operation>::value) {
         impacted_acc.insert( oho.result.get<object_id_type>() );
      }
      else {
         graphene::app::operation_get_impacted_accounts(op.op, impacted_acc, impacted_funds_tmp);
      }

      for (auto& a: other)
      {
         for (auto& item: a.account_auths) {
            impacted_acc.insert(item.first);
         }
      }

      // for each operation this account applies to that is in the config link it into the history
      if (_tracked_accounts.size() == 0)
      {
         for (auto& account_id: impacted_acc)
         {
            // we don't do index_account_keys here anymore, because
            // that indexing now happens in observers' post_evaluate()

            // add history
            const auto& stats_obj = account_id(db).statistics(db);
            const auto& ath = db.create<account_transaction_history_object>([&]( account_transaction_history_object& obj)
            {
               obj.operation_id = oho.id;
               obj.account      = account_id;
               obj.sequence     = stats_obj.total_ops+1;
               obj.next         = stats_obj.most_recent_op;
               obj.block_time   = b.timestamp;
            });
            db.modify(stats_obj, [&]( account_statistics_object& obj)
            {
               obj.most_recent_op = ath.id;
               obj.total_ops = ath.sequence;
            });
         }
      }
      else
      {
         for (auto account_id: _tracked_accounts)
         {
            if (impacted_acc.find(account_id) != impacted_acc.end())
            {
               // add history
               const auto& stats_obj = account_id(db).statistics(db);
               const auto& ath = db.create<account_transaction_history_object>([&](account_transaction_history_object& obj)
               {
                  obj.operation_id = oho.id;
                  obj.next         = stats_obj.most_recent_op;
                  obj.block_time   = b.timestamp;
               });
               db.modify( stats_obj, [&]( account_statistics_object& obj){
                  obj.most_recent_op = ath.id;
               });
            }
         }
      }

      /******** funds ********/

      // get the set of funds this operation applies to
      flat_set<account_id_type> impacted_acc_tmp;
      flat_set<fund_id_type> impacted_funds;
      app::operation_get_impacted_accounts(op.op, impacted_acc_tmp, impacted_funds);

      if (op.op.which() == operation::tag<fund_create_operation>::value) {
         impacted_funds.insert( oho.result.get<object_id_type>() );
      }

      for (const fund_id_type& fund_id: impacted_funds)
      {
         const auto& stats_obj = fund_id(db).statistics(db);
         const auto& ath = db.create<fund_transaction_history_object>([&](fund_transaction_history_object& obj)
         {
            obj.operation_id = oho.id;
            obj.fund         = fund_id;
            obj.sequence     = stats_obj.total_ops+1;
            obj.next         = stats_obj.most_recent_op;
            obj.block_time   = b.timestamp;
         });
         db.modify(stats_obj, [&]( fund_statistics_object& obj)
         {
            obj.most_recent_op = ath.id;
            obj.total_ops = ath.sequence;
         });
      }
   }
}
} // end namespace detail

account_history_plugin::account_history_plugin() :
   my( new detail::account_history_plugin_impl(*this) ) { }

account_history_plugin::~account_history_plugin() { }

std::string account_history_plugin::plugin_name() const {
   return "account_history";
}

void account_history_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg)
{
   cli.add_options()
         ("track-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to track history for (may specify multiple times)")
         ;
   cfg.add(cli);
}

void account_history_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   database().applied_block.connect( [&]( const signed_block& b){ my->update_account_histories(b); } );

   database().add_index<primary_index<operation_history_index>>();
   database().add_index<primary_index<account_transaction_history_index>>();
   database().add_index<primary_index<fund_transaction_history_index>>();

   LOAD_VALUE_SET(options, "tracked-accounts", my->_tracked_accounts, graphene::chain::account_id_type);
}

void account_history_plugin::plugin_startup() { }

flat_set<account_id_type> account_history_plugin::tracked_accounts() const {
   return my->_tracked_accounts;
}

} }
