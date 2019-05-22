#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( operation_tests_edc, database_fixture )

BOOST_AUTO_TEST_CASE( ban_objects_test )
{
   try {
      BOOST_TEST_MESSAGE( "=== ban_objects_test ===" );

      ACTOR(alice);
      ACTOR(bob);

      account_restrict_operation ar_op;

      ar_op.action = account_restrict_operation::restrict_all;
      ar_op.target = bob_id;

      trx.operations.push_back(ar_op);
      trx.validate();
      db.push_transaction(trx, ~0);

      const auto& idx = db.get_index_type<restricted_account_index>().indices().get<by_acc_id>();

      auto itr = idx.find(bob_id);

      BOOST_CHECK(itr != idx.end());
      BOOST_CHECK(db.get_index_type<restricted_account_index>().indices().size() == 1);

      trx.clear();

      account_restrict_operation ares_op;

      ares_op.action = account_restrict_operation::restore;
      ares_op.target = bob_id;

      trx.operations.push_back(ares_op);
      trx.validate();
      db.push_transaction(trx, ~0);

      itr = idx.find(bob_id);

      BOOST_CHECK(itr == idx.end());
      BOOST_CHECK(!db.get_index_type<restricted_account_index>().indices().size());

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( operations_restriction_test )
{
   BOOST_TEST_MESSAGE( "=== operations_restriction_test ===" );

   try {
      ACTOR(bob);

      BOOST_REQUIRE_NO_THROW(transfer(committee_account, bob_id, asset(1000000)));

      account_restrict_operation ar_op;

      ar_op.action = account_restrict_operation::restrict_all;
      ar_op.target = bob_id;

      trx.operations.push_back(ar_op);
      trx.validate();
      db.push_transaction(trx, ~0);

      BOOST_REQUIRE_THROW(transfer(committee_account, bob_id, asset(1000000)), fc::exception);
      BOOST_REQUIRE_THROW(transfer(bob_id, committee_account, asset(10000)), fc::exception);

      trx.clear();

      account_restrict_operation ares_op;

      ares_op.action = account_restrict_operation::restore;
      ares_op.target = bob_id;

      trx.operations.push_back(ares_op);
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.clear();

      BOOST_REQUIRE_NO_THROW(transfer(committee_account, bob_id, asset(1000000)));
      BOOST_REQUIRE_NO_THROW(transfer(bob_id, committee_account, asset(10000)));

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(burning_mode_test)
{
   BOOST_TEST_MESSAGE( "=== burning_mode_test ===" );

   try {

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      create_edc();

      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);

      issue_uia(alice_id, asset(30000, EDC_ASSET));

      // set burning mode for bob to true;
      set_burning_mode_operation op;
      op.account_id = bob_id;
      op.enabled = true;
      op.fee = asset();

      //set_expiration(db, trx);
      trx.operations.push_back(op);
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.clear();

      const asset_dynamic_data_object& asset_dynamic = edc_asset.dynamic_asset_data_id(db);
      share_type curr_supply_before = asset_dynamic.current_supply;

      transfer(alice_id, bob_id, asset(10000, edc_asset.id));

      // alice's balances must be less than before transfer
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 20000);

      // bob's balance must be without changes
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 0);

      // current supply must be decreased
      BOOST_CHECK((curr_supply_before - 10000) == asset_dynamic.current_supply);

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }

}

BOOST_AUTO_TEST_CASE(change_asset_fee_test)
{
   BOOST_TEST_MESSAGE( "=== change_asset_fee_test ===" );

   try {

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      create_edc(asset(100, CORE_ASSET), asset(2, EDC_ASSET));
      create_test_asset();

      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
      const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");

      // update test_asset's core_exchange_rate
      asset_update_exchange_rate_operation ex_upd_op;
      ex_upd_op.asset_to_update = test_asset.get_id();
      ex_upd_op.core_exchange_rate = price(asset(100, CORE_ASSET), asset(1, test_asset.get_id()));
      trx.operations.push_back(ex_upd_op);
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.clear();

      issue_uia(alice_id, asset(10000, EDC_ASSET));
      issue_uia(alice_id, asset(10000, test_asset.get_id()));

      generate_blocks(HARDFORK_623_TIME);
      generate_block();

      enable_fees();

      db.modify(global_property_id_type()(db), [](global_property_object& gpo)
      {
         gpo.parameters.current_fees->get<transfer_operation>().fee = 100;
         gpo.parameters.current_fees->get<transfer_operation>().price_per_kbyte = 0;
      });

      // transfer with fake fee
      set_expiration( db, trx );
      transfer_operation trans_fake_op;
      trans_fake_op.from   = alice_id;
      trans_fake_op.to     = bob_id;
      trans_fake_op.amount = asset(1, edc_asset.get_id());
      trans_fake_op.fee = asset(1, EDC_ASSET); // invalid fee amount
      trx.operations.push_back(trans_fake_op);
      sign(trx, alice_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx ), fc::exception );
      trx.operations.clear();

      // transfer with default commission (its conditions were set on asset creation stage)
      set_expiration( db, trx );
      transfer_operation trans_op;
      trans_op.from   = alice_id;
      trans_op.to     = bob_id;
      trans_op.amount = asset(1, edc_asset.get_id());
      trx.operations.push_back(trans_op);
      db.current_fee_schedule().set_fee(trx.operations[0], edc_asset.options.core_exchange_rate);
      trx.validate();
      db.push_transaction(trx, ~0);
      verify_asset_supplies(db);
      trx.operations.clear();

      // 1 EDC of transfer + 2 EDC of commission
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9997);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 1);

      // set new fee_payer asset for EDC-transfers
      set_expiration( db, trx );
      assets_update_fee_payer_operation payer_upd_op;
      payer_upd_op.assets_to_update = { edc_asset.get_id() };
      payer_upd_op.fee_payer_asset = test_asset.get_id();
      trx.operations.push_back(payer_upd_op);
      trx.validate();
      db.push_transaction(trx, ~0);
      verify_asset_supplies(db);
      trx.operations.clear();

      // another transfer. Now with commission of TEST_ASSET
      set_expiration( db, trx );
      transfer_operation trans2_op;
      trans2_op.from   = alice_id;
      trans2_op.to     = bob_id;
      trans2_op.amount = asset(1, edc_asset.get_id());
      trx.operations.push_back(trans2_op);
      db.current_fee_schedule().set_fee(trx.operations[0], test_asset.options.core_exchange_rate);
      trx.validate();
      db.push_transaction(trx, ~0);
      verify_asset_supplies(db);
      trx.operations.clear();

      // 1 EDC transfer + 2 EDC of commission + 1 EDC of transfer
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9996);
      // 1 TEST of commission
      BOOST_CHECK(get_balance(alice_id, test_asset.get_id()) == 9999);
      // now bob has 2 EDC because of two transfers
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 2);

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }

}

BOOST_AUTO_TEST_SUITE_END()
