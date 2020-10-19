#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/settings_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/witness_object.hpp>

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

      create_edc(10000000000, asset(100, CORE_ASSET), asset(2, EDC_ASSET));
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
         gpo.parameters.get_mutable_fees().get<transfer_operation>().fee = 100;
         gpo.parameters.get_mutable_fees().get<transfer_operation>().price_per_kbyte = 0;
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

BOOST_AUTO_TEST_CASE(blind_transfer2_test)
{
   BOOST_TEST_MESSAGE( "=== blind_transfer2_test ===" );

   try {

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      create_edc(10000000000, asset(100, CORE_ASSET), asset(2, EDC_ASSET));
      create_test_asset();

      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
      const settings_object& b_settings = db.get(settings_id_type(0));

      issue_uia(alice_id, asset(10000, EDC_ASSET));

      // change settings of blind_transfer2
      update_blind_transfer2_settings_operation s_op;
      s_op.blind_fee = asset(1, EDC_ASSET);
      trx.operations.push_back(s_op);
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.clear();

      generate_block();

      BOOST_CHECK(b_settings.blind_transfer_default_fee.amount == 1);

      const asset_dynamic_data_object& asset_dyn_data = edc_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(asset_dyn_data.current_supply.value == 10000);

      // make blind_transfer2
      blind_transfer2_operation op2;
      op2.from = alice_id;
      op2.to   = bob_id;
      op2.amount = asset(1000, EDC_ASSET);
      trx.operations.push_back(op2);
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.clear();

      generate_block();

      // 1 EDC is fee
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 8999);

      // 1 EDC burned (fee)
      BOOST_CHECK(asset_dyn_data.current_supply.value == 9999);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(market_addresses_test)
{
   BOOST_TEST_MESSAGE( "=== blind_transfer2_test ===" );

   try {

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      create_edc(10000000000, asset(100, CORE_ASSET), asset(2, EDC_ASSET));
      create_test_asset();

      issue_uia(alice_id, asset(10000, EDC_ASSET));

      // we need to represent the account as market first
      {
         set_market_operation op;
         op.to_account = bob_id;
         op.enabled = true;
         trx.operations.push_back(op);
         trx.validate();
         db.push_transaction(trx, ~0);
         trx.clear();
      }

      BOOST_CHECK(bob_id(db).is_market);

      // "notes" is too long
      {
         auto f = [&]()
         {
            create_market_address_operation op;
            op.market_account_id = bob_id;
            op.notes = "sdfsfwreewrsdfrewrwrwdfasfwersdfsfwreewrsdfrewrwrwdfasfwersdfsfwreewrsdfrewrwrwdfa";
            trx.operations.push_back(op);
            trx.validate();
            db.push_transaction(trx, ~0);
         };
         BOOST_CHECK_THROW(f(), fc::assert_exception);
         trx.clear();
      }

      {
         create_market_address_operation op;
         op.market_account_id = bob_id;
         op.notes = "sdfsfwreewrsdfrewrw";
         trx.operations.push_back(op);
         trx.validate();
         db.push_transaction(trx, ~0);
         trx.clear();
      }

      generate_block();

      const auto& idx = db.get_index_type<market_address_index>().indices().get<by_id>();
      BOOST_CHECK(idx.size() == 1);

      const market_address_object& addr_obj = *idx.begin();
      BOOST_CHECK(addr_obj.market_account_id == bob_id);

      {
         const auto& idx = db.get_index_type<market_address_index>().indices().get<by_address>().find(addr_obj.addr);
         BOOST_CHECK(idx->market_account_id == bob_id);
      }

      fc::time_point_sec h_time = HARDFORK_627_TIME + fc::days(1);
      generate_blocks(h_time);

      //std::cout << get_balance(bob_id, EDC_ASSET) << std::endl;
      //BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 20000);

      // can't create, because of fee
      {
         create_market_address_operation op;
         op.market_account_id = bob_id;
         op.notes = "sdfsfwreewrsdfrewrw";
         trx.operations.push_back(op);
         trx.validate();
         set_expiration(db, trx);
         BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::assert_exception);
         trx.clear();
      }

      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);

      issue_uia(bob_id, asset(10000, EDC_ASSET));

      generate_block();

      {
         create_market_address_operation op;
         op.market_account_id = bob_id;
         op.notes = "sdfsfwreewrsdfrewrw";
         trx.operations.push_back(op);
         trx.validate();
         set_expiration(db, trx);
         db.push_transaction(trx, ~0);
         trx.clear();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 9999);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(operation_history_test)
{
   BOOST_TEST_MESSAGE( "=== operation_history_test ===" );

   try {

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      create_edc(10000000000, asset(100, CORE_ASSET), asset(2, EDC_ASSET));
      create_test_asset();

      generate_block();

      const auto& idx = db.get_index_type<operation_history_index>().indices().get<by_id>();
      BOOST_CHECK(idx.size() == 5);

      issue_uia(alice_id, asset(10000, EDC_ASSET));

      generate_block();

      BOOST_CHECK(idx.size() == 6);

      {
         account_restrict_operation op;
         op.action = account_restrict_operation::restrict_all;
         op.target = alice_id;
         trx.operations.push_back(op);
         trx.validate();
         db.push_transaction(trx, ~0);
         trx.clear();
      }

      /**
       * daily_issue_operation doesn't produce history object because throws an
       * exception (alice is a restricted account now) */
      {
         daily_issue_operation op;
         op.issuer           = alice_id;
         op.asset_to_issue   = asset(10, EDC_ASSET);
         op.issue_to_account = alice_id;
         trx.operations.push_back(op);
         trx.validate();
         BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::assert_exception);
         trx.clear();
      }

      // we have still 6 history objects
      BOOST_CHECK(idx.size() == 6);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE(witness_fee_payments_test)
{
   BOOST_TEST_MESSAGE( "=== witness_fee_payments_test ===" );

   try {

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      asset_id_type edc_id = EDC_ASSET(db).get_id();
      issue_uia(bob_id, asset(30000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_633_TIME;
      generate_blocks(h_time);

      {
         update_settings_operation op;
         op.fee = asset();
         op.transfer_fees = {settings_fee{edc_id, 100}};
         op.edc_transfers_daily_limit = 3000000000;
         // 50% from all fees are paid to all witnesses
         op.extensions.value.witness_fees_percent = 50000;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      // we must consider 'witness.first_mt' flag, it should be disabled
      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      {
         transfer_operation op;
         op.fee = asset(20, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(20000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 9980);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      const account_object& acc_init0 = get_account("init0");

      BOOST_CHECK(get_balance(acc_init0.get_id(), EDC_ASSET) == 1);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
