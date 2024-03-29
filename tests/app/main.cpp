// see LICENSE.txt

#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>
#include <graphene/chain/balance_object.hpp>

#include <graphene/utilities/tempdir.hpp>
#include <graphene/history/history_plugin.hpp>
#include <graphene/chain/database.hpp>

#include <fc/thread/thread.hpp>

#include <boost/filesystem/path.hpp>

#include "../common/genesis_file_util.hpp"
#include "../common/database_fixture.hpp"

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

using namespace graphene;

BOOST_AUTO_TEST_CASE( two_node_network )
{
   using namespace graphene::chain;
   using namespace graphene::app;
   try {
      BOOST_TEST_MESSAGE( "=== two_node_network ===" );
      BOOST_TEST_MESSAGE( "Creating temporary files" );

      fc::temp_directory app_dir( graphene::utilities::temp_directory_path() );
      fc::temp_directory app2_dir( graphene::utilities::temp_directory_path() );
      fc::temp_file genesis_json;

      BOOST_TEST_MESSAGE( "Creating and initializing app1" );

      graphene::app::application app1;
      app1.register_plugin<graphene::history::history_plugin>();
      auto sharable_cfg = std::make_shared<boost::program_options::variables_map>();
      auto& cfg = *sharable_cfg;
      cfg.emplace("p2p-endpoint", boost::program_options::variable_value(string("127.0.0.1:3939"), false));
      cfg.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
      cfg.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
      app1.initialize(app_dir.path(), sharable_cfg);

      BOOST_TEST_MESSAGE( "Creating and initializing app2" );

      graphene::app::application app2;
      app2.register_plugin<history::history_plugin>();
      auto sharable_cfg2 = std::make_shared<boost::program_options::variables_map>();
      auto& cfg2 = *sharable_cfg2;
      cfg2.erase("p2p-endpoint");
      cfg2.emplace("p2p-endpoint", boost::program_options::variable_value(string("127.0.0.1:4040"), false));
      cfg2.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
      cfg2.emplace("seed-node", boost::program_options::variable_value(vector<string>{"127.0.0.1:3939"}, false));
      cfg2.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
      app2.initialize(app2_dir.path(), sharable_cfg2);

      BOOST_TEST_MESSAGE( "Starting app1 and waiting 1 s" );
      app1.startup();

      fc::usleep(fc::milliseconds(1000));
      BOOST_TEST_MESSAGE( "Starting app2 and waiting 1 s" );
      app2.startup();
      fc::usleep(fc::milliseconds(1000));

      BOOST_REQUIRE_EQUAL(app1.p2p_node()->get_connection_count(), (uint32_t)1);
      BOOST_CHECK_EQUAL(std::string(app1.p2p_node()->get_connected_peers().front().host.get_address()), "127.0.0.1");
      BOOST_TEST_MESSAGE( "app1 and app2 successfully connected" );

      std::shared_ptr<chain::database> db1 = app1.chain_database();
      std::shared_ptr<chain::database> db2 = app2.chain_database();

      BOOST_CHECK_EQUAL( db1->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 0 );
      BOOST_CHECK_EQUAL( db2->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 0 );

      account_id_type nathan_id = db2->get_index_type<account_index>().indices().get<by_name>().find( "nathan" )->id;
      fc::ecc::private_key nathan_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

      graphene::chain::signed_transaction trx_fee_payer;
      assets_update_fee_payer_operation payer_upd_op;
      payer_upd_op.assets_to_update = { CORE_ASSET };
      payer_upd_op.fee_payer_asset = CORE_ASSET;
      trx_fee_payer.operations.push_back(payer_upd_op);
      db1->current_fee_schedule().set_fee( trx_fee_payer.operations.back() );
      trx_fee_payer.set_expiration( db1->head_block_time() );
      trx_fee_payer.set_reference_block( db1->head_block_id() );
      trx_fee_payer.sign( nathan_key, db1->get_chain_id() );
      trx_fee_payer.validate();

      db1->push_transaction(trx_fee_payer, ~0);
      db2->push_transaction(trx_fee_payer, ~0);

      BOOST_TEST_MESSAGE( "Creating transfer tx" );

      graphene::chain::signed_transaction trx;
      {
         balance_claim_operation claim_op;
         balance_id_type bid = balance_id_type();
         claim_op.deposit_to_account = nathan_id;
         claim_op.balance_to_claim = bid;
         claim_op.balance_owner_key = nathan_key.get_public_key();
         claim_op.total_claimed = bid(*db1).balance;
         trx.operations.push_back( claim_op );
         db1->current_fee_schedule().set_fee( trx.operations.back() );

         transfer_operation xfer_op;
         xfer_op.from = nathan_id;
         xfer_op.to = GRAPHENE_NULL_ACCOUNT;
         xfer_op.amount = asset( 1000000, CORE_ASSET );
         trx.operations.push_back( xfer_op );
         db1->current_fee_schedule().set_fee( trx.operations.back() );

         const asset_object& core_asset = *db1->get_index_type<asset_index>().indices().get<by_symbol>().find("CORE");
         db1->current_fee_schedule().set_fee(trx.operations.back(), core_asset.options.core_exchange_rate);

         trx.set_expiration( db1->get_slot_time( 10 ) );
         trx.sign( nathan_key, db1->get_chain_id() );
         trx.validate();
      }

      BOOST_TEST_MESSAGE( "Pushing tx locally on db1" );
      processed_transaction ptrx = db1->push_transaction(trx);

      BOOST_CHECK_EQUAL( db1->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 1000000 );
      BOOST_CHECK_EQUAL( db2->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 0 );

      BOOST_TEST_MESSAGE( "Broadcasting tx" );
      app1.p2p_node()->broadcast(graphene::net::trx_message(trx));

      fc::usleep(fc::milliseconds(1000));

      BOOST_CHECK_EQUAL( db1->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 1000000 );
      BOOST_CHECK_EQUAL( db2->get_balance( GRAPHENE_NULL_ACCOUNT, CORE_ASSET ).amount.value, 1000000 );

      BOOST_TEST_MESSAGE( "Generating block on db2" );
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

      auto block_1 = db2->generate_block(
         db2->get_slot_time(1),
         db2->get_scheduled_witness(1),
         committee_key,
         database::skip_nothing);

      BOOST_TEST_MESSAGE( "Broadcasting block" );
      app2.p2p_node()->broadcast(graphene::net::block_message( block_1 ));

      fc::usleep(fc::milliseconds(1000));
      BOOST_TEST_MESSAGE( "Verifying nodes are still connected" );
      BOOST_CHECK_EQUAL(app1.p2p_node()->get_connection_count(), (uint32_t)1);
      BOOST_CHECK_EQUAL(app1.chain_database()->head_block_num(), (uint32_t)1);

      BOOST_TEST_MESSAGE( "Checking GRAPHENE_NULL_ACCOUNT has balance" );
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }

}
