#include <boost/test/unit_test.hpp>

#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/settings_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( fund_tests2, database_fixture )

BOOST_AUTO_TEST_CASE(hf_627_fees_test)
{

   try
   {

      BOOST_TEST_MESSAGE( "=== hf_627_fees_test ===" );

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(1000000, asset(100, CORE_ASSET), asset(1, EDC_ASSET));

      issue_uia(bob_id, asset(100000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_627_TIME;
      generate_blocks(h_time - fc::days(1));

      asset_id_type edc_id = EDC_ASSET(db).get_id();

      {
         update_settings_operation op;
         op.fee = asset();
         op.transfer_fees = {settings_fee{edc_id, 100}};
         op.blind_transfer_fees = {settings_fee{edc_id, 1000}};
         op.blind_transfer_default_fee = asset(1000, edc_id);
         op.edc_deposit_max_sum = 30000000000;
         op.edc_transfers_daily_limit = 30000000000;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      const settings_object& settings = *db.find(settings_id_type(0));
      BOOST_CHECK( (settings.transfer_fees[0].asset_id == edc_id) && (settings.transfer_fees[0].percent == 100) );
      BOOST_CHECK( (settings.blind_transfer_fees[0].asset_id == edc_id) && (settings.blind_transfer_fees[0].percent == 1000) );
      BOOST_CHECK( settings.blind_transfer_default_fee == asset(1000, edc_id) );
      BOOST_CHECK( settings.edc_deposit_max_sum == 30000000000 );
      BOOST_CHECK( settings.edc_transfers_daily_limit == 30000000000 );

      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);

      enable_fees();

      db.modify(global_property_id_type()(db), [](global_property_object& gpo)
      {
         gpo.parameters.current_fees->get<transfer_operation>().fee = 100;
         gpo.parameters.current_fees->get<transfer_operation>().price_per_kbyte = 0;
      });

      // transfer operation (before hf_627)
      {
         transfer_operation op;
         op.fee = asset(0, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(1000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         db.current_fee_schedule().set_fee(trx.operations[0], edc_asset.options.core_exchange_rate);
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      // blind_transfer2_operation (before hf_627)
      {
         blind_transfer2_operation op;
         op.fee = asset(0, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(1000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      // fee: 1 EDC (transfer, fixed 1 EDC) + 1000 EDC (blind_transfer, fixed 1000 EDC)
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 96999);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // transfer operation (after hf_627)
      {
         transfer_operation op;
         op.fee = asset(10, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(10000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      // blind_transfer2_operation (after hf_627)
      {
         blind_transfer2_operation op;
         op.fee = asset(10, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(1000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      /**
       * fee:
       * - 10 EDC (transfer_operation, 0.1% from 10000 of transfer amount);
       * - 10 EDC (blind_transfer, 1% from 1000 of transfer amount)
       */
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 85979);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(hf_627_transfers_limit_max_sum_test)
{

   try
   {
      BOOST_TEST_MESSAGE( "=== hf_627_transfers_limit_max_sum_test ===" );

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      asset_id_type edc_id = EDC_ASSET(db).get_id();
      const settings_object& settings = *db.find(settings_id_type(0));

      issue_uia(bob_id, asset(20000000000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_627_TIME;
      generate_blocks(h_time - fc::days(1));

      {
         transfer_operation op;
         op.fee = asset(0, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(4000000000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 16000000000);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // must throw exception, because bob has the limit flag in enabled state
      {
         transfer_operation op;
         op.fee = asset(0, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(settings.edc_transfers_daily_limit + 1, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::assert_exception);
         trx.clear();
      }

      // cancel the limit flag
      {
         account_limit_daily_volume_operation op;
         op.fee = asset(0, edc_id);
         op.account_id = bob_id;
         op.enabled = false;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      auto acc_bob = get_account("bob");
      BOOST_CHECK(acc_bob.edc_limit_daily_volume_enabled == false);

      for (int i = 0; i < 2; ++i)
      {
         transfer_operation op;
         op.fee = asset(0, edc_id);
         op.from = bob_id;
         op.to   = alice_id;
         op.amount = asset(4000000000, edc_id);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         trx.validate();
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 8000000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(fund_deposit_update_operation_test)
{

   try
   {
      BOOST_TEST_MESSAGE( "=== fund_deposit_update_operation_test ===" );

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      issue_uia(bob_id, asset(1000000, EDC_ASSET));

      generate_blocks(HARDFORK_627_TIME);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 5000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 5;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 10; // fund lifetime, days
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      generate_block();

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      {
         fund_deposit_operation op;
         op.amount = 10000;
         op.from_account = bob_id;
         op.period = 5;
         op.fund_id = fund.id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      fc::time_point_sec h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 990400); // 990000 + 400 (amount of payment)

      // deposit update: changing percent to 10% instead of 20%
      {
         fund_deposit_update_operation op;
         op.deposit_id = object_id_type(2, 21, 0);
         op.percent = 10000;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      // payment percent must be unchanged after applying 'deposit_renewal_operation'
      h_time = db.head_block_time() + fc::days(4);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // 990400 + 800 (4 days * 200, new amount of payment due to new percent)
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 991200);

      // deposit update: cancel percent
      {
         fund_deposit_update_operation op;
         op.deposit_id = object_id_type(2, 21, 0);
         op.reset = true;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // 991200 + 400 (old amount of payment)
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 991600);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 1385);

      // change deposit owner
      {
         fund_deposit_update_operation op;
         op.deposit_id = object_id_type(2, 21, 0);
         op.reset = true;
         op.extensions.value.account_id = alice_id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 991600);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 1879);

      BOOST_CHECK(db.get_index_type<fund_deposit_index>().indices().get<by_account_id>().count(bob_id) == 0);
      BOOST_CHECK(db.get_index_type<fund_deposit_index>().indices().get<by_account_id>().count(alice_id) == 1);

      const auto& stats_obj = fund.statistics_id(db);
      asset asst;

      {
         auto iter = stats_obj.users_deposits.find(bob_id);
         if (iter != stats_obj.users_deposits.end()) {
            asst = asset(iter->second, fund.asset_id);
         }
         BOOST_CHECK(asst.amount == 0);
      }

      {
         auto iter = stats_obj.users_deposits.find(alice_id);
         if (iter != stats_obj.users_deposits.end()) {
            asst = asset(iter->second, fund.asset_id);
         }
         BOOST_CHECK(asst.amount == 10000);
      }

      // change 'datetime_end' of deposit
      {
         // disable autorenewal
         enable_autorenewal_deposits_operation op_a;
         op_a.account_id = alice_id;
         op_a.enabled = false;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op_a));
         PUSH_TX(db, trx, ~0);
         trx.clear();

         fund_deposit_update_operation op;
         op.deposit_id = object_id_type(2, 21, 0);
         op.extensions.value.apply_extension_flags_only = true;
         op.extensions.value.datetime_end = db.head_block_time() + fc::seconds(120);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 12372);

      //std::cout << get_balance(alice_id, EDC_ASSET) << std::endl;

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(fund_deposit_leasing_limit_test)
{

   try
   {
      BOOST_TEST_MESSAGE("=== fund_deposit_leasing_limit_test ===");

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      issue_uia(bob_id, asset(50000000000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_627_TIME - fc::days(1);
      generate_blocks(h_time);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 5000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 5;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 10; // fund lifetime, days
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      generate_block();

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      for (int i = 0; i < 2; ++i)
      {
         fund_deposit_operation op;
         op.amount = 20000000000;
         op.from_account = bob_id;
         op.period = 5;
         op.fund_id = fund.id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 10000000000);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // fee: 1600000000 (2x * 1 day * 800000000)
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 11600000000);

      // wait until deposits make renewal and call 'rebuild_user_edc_deposit_availability'...
      h_time = db.head_block_time() + fc::days(4);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 17200000000);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }
      //std::cout << get_balance(bob_id, EDC_ASSET) << std::endl;

      // fee must be only 800000000 (from only first deposit)
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 18000000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(fund_deposit_reduce_operation_test)
{

   try
   {
      BOOST_TEST_MESSAGE( "=== fund_deposit_reduce_operation_test ===" );

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      issue_uia(bob_id, asset(1000000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_627_TIME;
      generate_blocks(h_time);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 5000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 5;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 10; // fund lifetime, days
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      {
         fund_deposit_operation op;
         op.amount = 10000;
         op.from_account = bob_id;
         op.period = 5;
         op.fund_id = fund.id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 990400);

      {
         fund_deposit_reduce_operation op;
         op.amount = 5000;
         op.deposit_id = object_id_type(2, 21, 0);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // + only half of the initial cost
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 990600);

      //std::cout << get_balance(bob_id, EDC_ASSET) << std::endl;

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()