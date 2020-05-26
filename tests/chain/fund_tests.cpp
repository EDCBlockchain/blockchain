#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/hardfork.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( fund_tests, database_fixture )

BOOST_AUTO_TEST_CASE(create_fund_test)
{

   try
   {
      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      BOOST_CHECK(fund.name == "TESTFUND");
      BOOST_CHECK(fund.period == 100);
      BOOST_CHECK(fund.rates_reduction_per_month == 300);
      BOOST_CHECK(fund.fund_rates[0].amount == 10000);
      BOOST_CHECK(fund.fund_rates[0].day_percent == 1000);
      BOOST_CHECK(fund.payment_rates[0].period == 50);
      BOOST_CHECK(fund.payment_rates[0].percent == 20000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_fund_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== update_fund_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      // new settings
      fund_options::fund_rate fr2;
      fr2.amount = 2000;
      fr2.day_percent = 80;

      fund_options::payment_rate pr2;
      pr2.period = 60;
      pr2.percent = 2;

      fund_options options_new;
      options_new.min_deposit = 20000;
      options_new.rates_reduction_per_month = 20;
      options_new.period = 60;
      options_new.description = "FUNDUPDATED";
      options_new.fund_rates.push_back(std::move(fr2));
      options_new.payment_rates.push_back(std::move(pr2));

      fund_update_operation uop;
      uop.id = fund.get_id();
      uop.options = options_new;

      set_expiration(db, trx);
      trx.operations.push_back(uop);
      PUSH_TX(db, trx, ~0);
      trx.clear();

      // new updated states
      BOOST_CHECK(fund.description == "FUNDUPDATED");
      BOOST_CHECK(fund.period == 60);
      BOOST_CHECK(fund.min_deposit == 20000);
      BOOST_CHECK(fund.fund_rates[0].amount == 2000);
      BOOST_CHECK(fund.fund_rates[0].day_percent == 80);
      BOOST_CHECK(fund.payment_rates[0].period == 60);
      BOOST_CHECK(fund.payment_rates[0].percent == 2);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fund_refill_test )
{
   try
   {
      /**
       * 1) for refill operation we create 'alice' account;
       * 2) make refill from 'alice' to fund;
       * 3) check balances of a fund and 'alice';
       */
      BOOST_TEST_MESSAGE( "=== fund_refill_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(alice_id, asset(10000000, EDC_ASSET));
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000000);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      // check fund balance before refilling...
      BOOST_CHECK(fund.balance == 0);

      fund_refill_operation fro;
      fro.fee = asset();
      fro.from_account = alice_id;
      fro.id = fund.get_id();
      fro.amount = 10000;

      trx.clear();
      set_expiration(db, trx);
      trx.operations.push_back(std::move(fro));
      PUSH_TX( db, trx, ~0 );

      // check fund & alice balances after refilling...
      BOOST_CHECK(fund.balance == 10000);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9990000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fund_deposit_test )
{
   try
   {
      /**
       * 1) for deposit operation we create 'alice' account;
       * 2) make deposit from 'alice' to fund;
       * 3) check balances of a fund and 'alice'
       */
      BOOST_TEST_MESSAGE( "=== fund_deposit_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(alice_id, asset(10000000, EDC_ASSET));
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000000);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      // check fund balance before deposit...
      BOOST_CHECK(fund.balance == 0);

      fund_deposit_operation fdo;
      fdo.amount = 10000;
      fdo.fee = asset();
      fdo.from_account = alice_id;
      fdo.period = 50;
      fdo.fund_id = fund.id;
      set_expiration(db, trx);
      trx.operations.push_back(std::move(fdo));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      // check fund & alice balances after deposit...
      BOOST_CHECK(fund.balance == 10000);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9990000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fund_get_max_fund_rate_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== fund_get_max_fund_rate_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      // payments to fund
      fund_options::fund_rate fr1;
      fr1.amount = 10000;
      fr1.day_percent = 1000;
      fund_options::fund_rate fr2;
      fr2.amount = 3000;
      fr2.day_percent = 1000;
      fund_options::fund_rate fr3;
      fr3.amount = 1800;
      fr3.day_percent = 1000;
      fund_options::fund_rate fr4;
      fr4.amount = 6589;
      fr4.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr1));
      options.fund_rates.push_back(std::move(fr2));
      options.fund_rates.push_back(std::move(fr3));
      options.fund_rates.push_back(std::move(fr4));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      // std::cout << "!!!: " << fund.fund_rates.size() << std::endl;
      // for (const fund_options::fund_rate& f: fund.fund_rates) {
      //   std::cout << "amount: " << f.amount.value << ", day_percent: " << f.day_percent << std::endl;
      //}

      optional<fund_options::fund_rate> f_rate = fund.get_max_fund_rate(1000);
      BOOST_CHECK(!f_rate.valid());

      f_rate = fund.get_max_fund_rate(20000);
      BOOST_CHECK(f_rate.valid());
      BOOST_CHECK(f_rate->amount == 10000);

      f_rate = fund.get_max_fund_rate(10000);
      BOOST_CHECK(f_rate.valid());
      BOOST_CHECK(f_rate->amount == 10000);

      f_rate = fund.get_max_fund_rate(3002);
      BOOST_CHECK(f_rate.valid());
      BOOST_CHECK(f_rate->amount == 3000);

      f_rate = fund.get_max_fund_rate(9000);
      BOOST_CHECK(f_rate.valid());
      BOOST_CHECK(f_rate->amount == 6589);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fund_make_payments_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== fund_make_payments_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(alice_id, asset(10000, EDC_ASSET));
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);

      issue_uia(bob_id, asset(10000, EDC_ASSET));
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 10000);

      // disable autorenewal
      enable_autorenewal_deposits_operation op_a;
      op_a.account_id = bob_id;
      op_a.enabled = false;
      set_expiration(db, trx);
      trx.operations.push_back(std::move(op_a));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      generate_blocks(HARDFORK_622_TIME);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 50;
      pr.percent = 20000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 300;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      BOOST_CHECK(fund_iter != db.get_index_type<fund_index>().indices().get<by_name>().end());
      const fund_object& fund = *fund_iter;

      // alice, fund owner: refilling with full its amount
      fund_refill_operation fro;
      fro.amount = 10000; // minimal available deposit
      fro.fee = asset();
      fro.from_account = alice_id;
      fro.id = fund.get_id();
      trx.operations.push_back(std::move(fro));
      PUSH_TX(db, trx, ~0);
      trx.clear();
      BOOST_CHECK(fund.balance == 10000);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 0);

      // bob: deposit
      fund_deposit_operation fdo;
      fdo.amount = 10000;
      fdo.fee = asset();
      fdo.from_account = bob_id;
      fdo.period = 50;
      fdo.fund_id = fund.get_id();
      trx.operations.push_back(std::move(fdo));
      PUSH_TX(db, trx, ~0);
      trx.clear();
      BOOST_CHECK(fund.balance == 20000);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 0);

      // first maintenance_time
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // history
      BOOST_CHECK(fund.history_id(db).items.size() == 1);
      BOOST_CHECK(fund.history_id(db).items[0].create_datetime.sec_since_epoch() == db.head_block_time().sec_since_epoch());
      BOOST_CHECK(fund.history_id(db).items[0].daily_payments_total.value == 200);
      BOOST_CHECK(fund.history_id(db).items[0].daily_payments_without_owner.value == 40);
      BOOST_CHECK(fund.history_id(db).items[0].daily_payments_owner.value == 160);

      // std::cout << "========= 1 alice's balance: " << get_balance(alice_id, EDC_ASSET) << std::endl;
      // std::cout << "========= 1 bob's balance: " << get_balance(bob_id, EDC_ASSET) << std::endl;

      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 160);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 40);

//      fc::time_point_sec h_time = db.head_block_time();
//
//      for (int i = 50, ii = 1; i > 0; --i, ++ii)
//      {
//         while (db.head_block_time() < (h_time + fc::days(1))) {
//            generate_block();
//         }
//         h_time = db.head_block_time();
//         std::cout << "day #" << ii << "========= 2 alice's balance: " << get_balance(alice_id, EDC_ASSET) << std::endl;
//      }

      // until bob's deposit is actual
      fc::time_point_sec h_time = db.head_block_time() + fc::days(49);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // history
      BOOST_CHECK(fund.history_id(db).items.size() == 50);

      // std::cout << "========= 2 alice's balance: " << get_balance(alice_id, EDC_ASSET) << std::endl;
      // std::cout << "========= 2 bob's balance: " << get_balance(bob_id, EDC_ASSET) << std::endl;

      // fund has paid 10000 EDC & percent to bob, because his deposit has already finished
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 12000);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 5550);

      h_time = db.head_block_time() + fc::days(50);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // std::cout << "========= 3 alice's balance: " << get_balance(alice_id, EDC_ASSET) << std::endl;
      // std::cout << "========= 3 bob's balance: " << get_balance(bob_id, EDC_ASSET) << std::endl;

      // bob still has old balance
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 12000);
      // alice has fund amount + percent
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 16825);

      BOOST_CHECK(!fund.enabled);
      BOOST_CHECK(fund.balance == 0);

      /************* owner's fixed percent **************/

      // enable fund
      fund_set_enable_operation en_op;
      en_op.id      = fund.get_id();
      en_op.enabled = true;
      set_expiration(db, trx);
      trx.operations.push_back(std::move(en_op));
      PUSH_TX(db, trx, ~0);
      trx.clear();
      BOOST_CHECK(fund.enabled);

      /**
       * update fund options: we need new period of it and
       * new payment_rate-period
       */
      {
         fund_options::fund_rate fr2;
         fr2.amount = 10000;
         fr2.day_percent = 1000;
         // payments to users
         fund_options::payment_rate pr2;
         pr2.period = 50;
         pr2.percent = 40000;

         fund_options options2;
         options2.description = "FUND DESCRIPTION";
         options2.period = 102;
         options2.min_deposit = 10000;
         options2.rates_reduction_per_month = 0;
         options2.fund_rates.push_back(std::move(fr2));
         options2.payment_rates.push_back(std::move(pr2));

         fund_update_operation uop2;
         uop2.id = fund.get_id();
         uop2.options = options2;
         set_expiration(db, trx);
         trx.operations.push_back(uop2);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      // refill bob's balance
      issue_uia(bob_id, asset(18000, EDC_ASSET));
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 30000);

      // bob: make deposit #2
      fund_deposit_operation fdo2;
      fdo2.amount = 30000;
      fdo2.fee = asset();
      fdo2.from_account = bob_id;
      fdo2.period = 50;
      fdo2.fund_id = fund.get_id();
      set_expiration(db, trx);
      trx.operations.push_back(std::move(fdo2));
      PUSH_TX(db, trx, ~0);
      trx.clear();
      BOOST_CHECK(fund.balance == 30000);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 0);

      // 'fund_change_payment_scheme_operation'
      fund_change_payment_scheme_operation sf_op;
      sf_op.id = fund.get_id();
      sf_op.payment_scheme = 1;
      set_expiration(db, trx);
      trx.operations.push_back(std::move(sf_op));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      BOOST_CHECK(fund.payment_scheme == fund_payment_scheme::fixed);

      /**
       * at current moment:
       * alice==16825
       * bob==0
       */
      // std::cout << "bob balance:" << get_balance(bob_id, EDC_ASSET) << std::endl;
      // std::cout << "alice balance:" << get_balance(alice_id, EDC_ASSET) << std::endl;

      // refill fund
      {
         fund_refill_operation fro;
         fro.fee = asset();
         fro.from_account = alice_id;
         fro.id = fund.get_id();
         fro.amount = 10000;

         trx.clear();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fro));
         PUSH_TX(db, trx, ~0);

         BOOST_CHECK(fund.balance == 40000);
         BOOST_CHECK(fund.owner_balance == 10000);
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }
      // std::cout << "bob balance:" << get_balance(bob_id, EDC_ASSET) << std::endl;
      // std::cout << "alice balance:" << get_balance(alice_id, EDC_ASSET) << std::endl;

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 240);
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 6827);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fund_make_autorenewal_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== fund_make_autorenewal_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(bob_id, asset(50000, EDC_ASSET));

      //generate_blocks(HARDFORK_624_TIME);
      generate_blocks(HARDFORK_626_TIME);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 1;
      pr.percent = 1000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 10;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 0;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      BOOST_CHECK(bob.deposits_autorenewal_enabled);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 50000);

      fund_deposit_operation fdo1;
      fdo1.amount = 10000;
      fdo1.fee = asset();
      fdo1.from_account = bob_id;
      fdo1.period = 1;
      fdo1.fund_id = fund.get_id();
      set_expiration(db, trx);
      trx.operations.push_back(std::move(fdo1));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      const auto& idx_deposits = db.get_index_type<fund_deposit_index>().indices().get<by_id>();
      BOOST_CHECK(idx_deposits.size() == 1);

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 40000);

      // user payment should work every 3 days
      fc::time_point_sec h_time = db.head_block_time() + fc::days(3);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 40300);

      // disable autorenewal
      enable_autorenewal_deposits_operation op;
      op.account_id = bob_id;
      op.enabled = false;
      set_expiration(db, trx);
      trx.operations.push_back(std::move(op));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      BOOST_CHECK(!get_account("bob").deposits_autorenewal_enabled);

      // payment to user should work only once (+amount of deposit) despite of 3 days
      h_time = db.head_block_time() + fc::days(3);
      while (db.head_block_time() < h_time) {
         generate_block();
      }
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 50400);

      // deposit must be already deleted
      //BOOST_CHECK(idx_deposits.size() == 0);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( funds_hardfork_626_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== funds_hardfork_626_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(alice);
      ACTOR(bob);

      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(bob_id, asset(50000, EDC_ASSET));

      // generate blocks before HARDFORK_626_TIME (-2 day)
      generate_blocks(HARDFORK_626_TIME - (86400 * 2));

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 1;
      pr.percent = 1000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 10;
      options.min_deposit = 10000;
      options.rates_reduction_per_month = 0;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      fund_deposit_operation fdo1;
      fdo1.amount = 10000;
      fdo1.fee = asset();
      fdo1.from_account = bob_id;
      fdo1.period = 1;
      fdo1.fund_id = fund.get_id();
      set_expiration(db, trx);
      trx.operations.push_back(std::move(fdo1));
      PUSH_TX(db, trx, ~0);
      trx.clear();

      // get_balance(bob_id, EDC_ASSET) == 40000

      // 1 payment to bob: generate blocks before HARDFORK_626_TIME (-1 day)
      fc::time_point_sec h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 40100);

      // generate blocks before HARDFORK_626_TIME (0 day)
      h_time = h_time + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 40200);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( funds_deposits_limit )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== funds_deposits_limit ===" );

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)
      ACTOR(eve)

      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      issue_uia(alice_id, asset(50000000000, EDC_ASSET));

      // disable autorenewal
      {
         enable_autorenewal_deposits_operation op_a;
         op_a.account_id = alice_id;
         op_a.enabled = false;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op_a));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_blocks(HARDFORK_627_TIME);
      generate_block();

      // fund1
      {
         fund_options::fund_rate fr;
         fr.amount = 10000;
         fr.day_percent = 1000;
         // payments to users
         fund_options::payment_rate pr;
         pr.period = 2;
         pr.percent = 1000;

         fund_options options;
         options.period = 10;
         options.min_deposit = 30000;
         options.rates_reduction_per_month = 0;
         options.fund_rates.push_back(std::move(fr));
         options.payment_rates.push_back(std::move(pr));
         make_fund("FUND1", options, bob_id);
      }

      // fund2
      {
         fund_options::fund_rate fr;
         fr.amount = 10000;
         fr.day_percent = 1000;
         // payments to users
         fund_options::payment_rate pr;
         pr.period = 1;
         pr.percent = 1000;

         fund_options options;
         options.period = 10;
         options.min_deposit = 30000;
         options.rates_reduction_per_month = 0;
         options.fund_rates.push_back(std::move(fr));
         options.payment_rates.push_back(std::move(pr));
         make_fund("FUND2", options, eve_id);
      }

      auto fund_iter1 = db.get_index_type<fund_index>().indices().get<by_name>().find("FUND1");
      const fund_object& fund1 = *fund_iter1;

      // deposit to fund1
      {
         fund_deposit_operation fdo;
         fdo.amount = 30000000000;
         fdo.fee = asset();
         fdo.from_account = alice_id;
         fdo.period = 2;
         fdo.fund_id = fund1.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();

      BOOST_CHECK(alice.edc_in_deposits.value == 30000000000);

      fc::time_point_sec h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      auto fund_iter2 = db.get_index_type<fund_index>().indices().get<by_name>().find("FUND2");
      const fund_object& fund2 = *fund_iter2;

      // deposit to fund2, attempt 1
      {
         fund_deposit_operation fdo;
         fdo.amount = 20000000000;
         fdo.fee = asset();
         fdo.from_account = alice_id;
         fdo.period = 1;
         fdo.fund_id = fund2.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::assert_exception);
         trx.clear();
      }

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      // deposit to fund2, attempt 2
      {
         fund_deposit_operation fdo;
         fdo.amount = 20000000000;
         fdo.fee = asset();
         fdo.from_account = alice_id;
         fdo.period = 1;
         fdo.fund_id = fund2.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( funds_payments_limit )
{
   try
   {
      BOOST_TEST_MESSAGE("=== funds_payments_limit ===");

      ACTOR(abcde1) // for needed IDs
      ACTOR(alice)
      ACTOR(bob)

      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(100000000000);

      issue_uia(bob_id, asset(50000000000, EDC_ASSET));

      fc::time_point_sec h_time = HARDFORK_627_TIME - fc::days(5);
      generate_blocks(h_time);

      // payments to fund
      fund_options::fund_rate fr;
      fr.amount = 1000000000;
      fr.day_percent = 1000;
      // payments to users
      fund_options::payment_rate pr;
      pr.period = 10;
      pr.percent = 1000;

      fund_options options;
      options.description = "FUND DESCRIPTION";
      options.period = 100; // fund existence period, days
      options.min_deposit = 1000000000;
      options.rates_reduction_per_month = 0;
      options.fund_rates.push_back(std::move(fr));
      options.payment_rates.push_back(std::move(pr));
      make_fund("TESTFUND", options, alice_id);

      auto fund_iter = db.get_index_type<fund_index>().indices().get<by_name>().find("TESTFUND");
      const fund_object& fund = *fund_iter;

      // disable autorenewal
      {
         enable_autorenewal_deposits_operation op_a;
         op_a.account_id = bob_id;
         op_a.enabled = false;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op_a));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      for (int i = 0; i < 30; ++i)
      {
         fund_deposit_operation fdo;
         fdo.amount = 1000000000; // 1 mln
         fdo.fee = asset();
         fdo.from_account = bob_id;
         fdo.period = 10;
         fdo.fund_id = fund.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         PUSH_TX(db, trx, ~0);
         trx.clear();

         generate_block();
      }

      h_time = db.head_block_time() + fc::days(5);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      //std::cout << get_balance(bob_id, EDC_ASSET) << std::endl;
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 20150000000);
      BOOST_CHECK(db.get_user_deposits_sum(bob_id, EDC_ASSET).value == 30000000000);

      // must throw, because 30m exceeded
      {
         fund_deposit_operation fdo;
         fdo.amount = 1000000000; // 1 mln
         fdo.fee = asset();
         fdo.from_account = bob_id;
         fdo.period = 10;
         fdo.fund_id = fund.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::assert_exception);
         trx.clear();
      }

      // make withdrawals to bob
      h_time = db.head_block_time() + fc::days(5);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(db.get_user_deposits_sum(bob_id, EDC_ASSET).value == 0);

      {
         fund_deposit_operation fdo;
         fdo.amount = 1000000000; // 1 mln
         fdo.fee = asset();
         fdo.from_account = bob_id;
         fdo.period = 10;
         fdo.fund_id = fund.get_id();
         set_expiration(db, trx);
         trx.operations.push_back(std::move(fdo));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      BOOST_CHECK(db.get_user_deposits_sum(bob_id, EDC_ASSET).value == 1000000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()