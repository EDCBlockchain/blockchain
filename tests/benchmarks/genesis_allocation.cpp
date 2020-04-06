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
#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/utilities/tempdir.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/test/auto_unit_test.hpp>

#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( genesis_allocation, database_fixture )

uint32_t CHAIN_BENCH_GRAPHENE_TESTING_GENESIS_TIMESTAMP = 1431700000;
genesis_state_type make_genesis()
{
   genesis_state_type genesis_state;

   genesis_state.initial_timestamp = time_point_sec( CHAIN_BENCH_GRAPHENE_TESTING_GENESIS_TIMESTAMP );

   auto init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
   genesis_state.initial_active_witnesses = 10;
   for (uint64_t i = 0; i < genesis_state.initial_active_witnesses; ++i)
   {
      const std::string& name = "init" + fc::to_string(i);
      genesis_state.initial_accounts.emplace_back(name
                                                  , init_account_priv_key.get_public_key()
                                                  , init_account_priv_key.get_public_key()
                                                  , true);
      genesis_state.initial_committee_candidates.push_back({name});
      genesis_state.initial_witness_candidates.push_back({name, init_account_priv_key.get_public_key()});
   }
   genesis_state.initial_parameters.get_mutable_fees().zero_all_fees();
   return genesis_state;
}

BOOST_AUTO_TEST_CASE( operation_sanity_check )
{
   try {

      BOOST_TEST_MESSAGE( "=== operation_sanity_check ===" );

      operation op = account_create_operation();
      op.get<account_create_operation>().active.add_authority(account_id_type(), 123);
      operation tmp = std::move(op);
      wdump((tmp.which()));
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( genesis_and_persistence_bench )
{
   try {

      BOOST_TEST_MESSAGE( "=== genesis_and_persistence_bench ===" );

      genesis_state_type genesis_state = make_genesis();

#ifdef NDEBUG
      ilog("Running in release mode.");

      //const int account_count = 20000;
      const int account_count = 200;
      //const int blocks_to_produce = 100000;
      const int blocks_to_produce = 100;
#else
      ilog("Running in debug mode.");
      const int account_count = 30000;
      const int blocks_to_produce = 1000;
#endif

      fc::temp_directory data_dir( graphene::utilities::temp_directory_path() );

      {
         db.close();
         db.open(data_dir.path(), [&]{return genesis_state;});

         BOOST_TEST_MESSAGE("create additional accounts...");

         std::vector<account_id_type> accounts_new;
         accounts_new.reserve(account_count);
         for (int i = 0; i < account_count; ++i)
         {
            const std::string& name = "target" + std::to_string(i);
            fc::ecc::private_key priv_key = generate_private_key(name);
            public_key_type pub_key = priv_key.get_public_key();
            const account_object& acc_obj = create_account(name, pub_key);

            accounts_new.emplace_back(acc_obj.get_id());
         }

         create_test_asset();
         const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");
         int64_t acc_balance = test_asset.options.max_supply.value / account_count;

         BOOST_TEST_MESSAGE("done.");
         BOOST_TEST_MESSAGE("set account balances...");

         for (const account_id_type& acc_id: accounts_new)
         {
            signed_transaction trx;
            set_expiration( db, trx );

            asset_issue_operation op;
            op.issuer = test_asset.issuer;
            op.asset_to_issue = asset(acc_balance, test_asset.get_id());
            op.issue_to_account = acc_id;
            trx.operations = {op};
            trx.validate();
            db.push_transaction(trx, ~0);
         }

         BOOST_TEST_MESSAGE("done.");
         BOOST_TEST_MESSAGE("check account balances...");

         for (const account_id_type& acc_id: accounts_new) {
            BOOST_CHECK(db.get_balance(acc_id, test_asset.get_id()).amount.value == (test_asset.options.max_supply.value / account_count));
         }

         fc::time_point start_time = fc::time_point::now();
         db.close();
         BOOST_TEST_MESSAGE("closed database in " << ((fc::time_point::now() - start_time).count() / 1000) << " milliseconds.");
      }

      {
         db.close();

         fc::time_point start_time = fc::time_point::now();
         db.open(data_dir.path(), [&]{return genesis_state;});
         BOOST_TEST_MESSAGE("opened database in " << ((fc::time_point::now() - start_time).count() / 1000) << " milliseconds.");

         int blocks_out = 0;
         auto witness_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
         auto aw = db.get_global_properties().active_witnesses;
         auto b = db.generate_block( db.get_slot_time( 1 ), db.get_scheduled_witness( 1 ), witness_priv_key, ~0 );

         create_test_asset();
         const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");

         // additional account for transactions
         fc::ecc::private_key priv_key = generate_private_key("key");
         public_key_type pub_key = priv_key.get_public_key();
         const account_object& acc_obj = create_account("target", pub_key);

         {
            signed_transaction trx;
            set_expiration(db, trx);
            asset_issue_operation op;
            op.issuer = test_asset.issuer;
            op.asset_to_issue = asset(test_asset.options.max_supply, test_asset.get_id());
            op.issue_to_account = acc_obj.get_id();
            trx.operations = {op};
            trx.validate();
            db.push_transaction(trx, ~0);
         }

//         db.modify(global_property_id_type()(db), [](global_property_object& gpo) {
//            gpo.parameters.maintenance_interval = 30 * 24 * 60 * 60; // 30 days
//         });

         BOOST_TEST_MESSAGE("done.");
         BOOST_TEST_MESSAGE("generating blocks...");

         start_time = fc::time_point::now();
         for (int i = 0; i < blocks_to_produce; ++i)
         {
            if (blocks_out % 10000 == 0) {
               BOOST_TEST_MESSAGE("blocks_count: " + std::to_string(blocks_out));
            }

            signed_transaction trx;
            set_expiration(db, trx);
            transfer_operation op;
            op.from = acc_obj.get_id();
            op.to   = account_id_type(0);
            op.amount = asset(1, test_asset.get_id());
            trx.operations = {op};
            db.push_transaction(trx, ~0);

            aw = db.get_global_properties().active_witnesses;
            b =  db.generate_block( db.get_slot_time( 1 ), db.get_scheduled_witness( 1 ), witness_priv_key, ~0 );
            ++blocks_out;
         }
         BOOST_TEST_MESSAGE("done.");
         BOOST_TEST_MESSAGE("pushed " << blocks_out << " blocks (1 op each, no validation) in "
                                      << ((fc::time_point::now() - start_time).count() / 1000) << " milliseconds.");

         start_time = fc::time_point::now();
         db.close();
         BOOST_TEST_MESSAGE("closed database in " << ((fc::time_point::now() - start_time).count() / 1000) << " milliseconds.");
      }
   }
   catch(fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()