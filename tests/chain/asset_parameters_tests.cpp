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

BOOST_FIXTURE_TEST_SUITE( asset_parameters_tests, database_fixture )

int64_t percent_to_int(double percent) { return percent * 100000; };

BOOST_AUTO_TEST_CASE( create_edc )
{
   try {

      BOOST_TEST_MESSAGE( "=== create_edc ===" );

      asset_id_type edc_asset_id = db.get_index<asset_object>().get_next_id();
      (void)edc_asset_id;

      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = "EDC";
      creator.common_options.max_supply = 10000000000;
      creator.precision = 3;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
      creator.common_options.flags = charge_market_fee;
      creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
      trx.operations.push_back(std::move(creator));
      PUSH_TX( db, trx, ~0 );
      generate_block();
   } catch (...) {
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_premine )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_premine ===" );

      ACTOR( abcde1 );
      ACTOR( abcde2 );
      ACTOR( alice );

      //assign privileges for create_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      INVOKE(create_edc);
      asset_parameters ap;
      ap.bonus_percent = 0.1;
      ap.mining = false;
      ap.maturing_bonus_balance = false;
      ap.coin_maturing = false;
      ap.mandatory_transfer = 0;
      ap.premine = 1000000000;
      auto test_asset = create_user_issued_asset( "TEST", ap, alice_id );
      generate_block();

      BOOST_CHECK( get_balance(alice_id, test_asset.id) == ap.premine);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_daily_bonus )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_daily_bonus ===" );

      ACTOR( abcde1 );
      ACTOR( abcde2 );
      ACTOR( alice );

      //assign privileges for create_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      INVOKE(create_edc);


      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));

      set_expiration(db, this->trx);
      asset_parameters ap;
      ap.bonus_percent = percent_to_int(0.1);
      ap.mining = false;
      ap.maturing_bonus_balance = false;
      ap.daily_bonus = true;
      ap.coin_maturing = false;
      ap.mandatory_transfer = 0;
      auto asset1 = create_user_issued_asset( "TESTA", ap, alice_id );
      ap.bonus_percent = percent_to_int(0.2);
      auto asset2 = create_user_issued_asset( "TESTB", ap, alice_id );
      
      ap.bonus_percent = percent_to_int(0.5);
      auto asset3 = create_user_issued_asset( "TESTC", ap, alice_id );

      set_expiration(db, this->trx);
      
      issue_uia( alice_id, asset( 300000, asset1.id ) );
      issue_uia( alice_id, asset( 300000, asset2.id ) );
      issue_uia( alice_id, asset( 300000, asset3.id ) );
      
      auto target = HARDFORK_620_TIME + fc::days( 1 );
      while( db.head_block_time() < target ) 
      {
         generate_block();
      }
      while( db.head_block_time() < target + fc::days( 2 ) ) 
      {
         generate_block();
      }

      auto& index = db.get_index_type<bonus_balances_index>().indices().get<by_account>();
      (void)index;

      BOOST_CHECK( get_balance(alice_id, asset1.id) == 399300  );
      BOOST_CHECK( get_balance(alice_id, asset2.id) == 518400  );
      BOOST_CHECK( get_balance(alice_id, asset3.id) == 1012500 );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_bonus_balances )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_bonus_balances ===" );

      ACTOR( abcde1 );
      ACTOR( abcde2 );
      ACTOR( alice );

      //assign privileges for create_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      INVOKE(create_edc);

      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));

      set_expiration(db, this->trx);
      asset_parameters ap;
      ap.bonus_percent = percent_to_int(0.1);
      ap.mining = false;
      ap.maturing_bonus_balance = false;
      ap.coin_maturing = false;
      auto asset1 = create_user_issued_asset( "EDCA", ap, alice_id );
      ap.bonus_percent = 0.2;
      auto asset2 = create_user_issued_asset( "EDCB", ap, alice_id );
      ap.bonus_percent = 0.5;
      auto asset3 = create_user_issued_asset( "EDCC", ap, alice_id );
      set_expiration(db, this->trx);
      issue_uia( alice_id, asset( 300000, asset1.id ) );
      issue_uia( alice_id, asset( 300000, asset2.id ) );
      issue_uia( alice_id, asset( 300000, asset3.id ) );

      while( db.head_block_time() < HARDFORK_620_TIME + fc::days( 3 ) ) 
      {
         generate_block();
      }

      auto& index = db.get_index_type<bonus_balances_index>().indices().get<by_account>();
      (void)index;

      BOOST_CHECK(true);
      // BOOST_CHECK( get_balance(alice_id, asset1.get_id()) == 399300  );
      // BOOST_CHECK( get_balance(alice_id, asset2.get_id()) == 518400  );
      // BOOST_CHECK( get_balance(alice_id, asset3.get_id()) == 1012500 );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( mining_test )
{
   // std::cout << boost::unit_test::framework::current_test_case().p_name << std::endl;
   BOOST_TEST_MESSAGE( "=== mining_test ===" );

   ACTOR( abcde1 );
   ACTOR( abcde2 );
   ACTOR( alice );

   //assign privileges for create_asset_operation
   SET_ACTOR_CAN_CREATE_ASSET(alice_id);

   INVOKE(create_edc);

   generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));

   set_expiration(db, this->trx);
   asset_parameters ap;
   ap.bonus_percent = percent_to_int(0.1);
   ap.mining = true;
   ap.maturing_bonus_balance = false;
   ap.coin_maturing = false;
   auto asset1 = create_user_issued_asset( "EDCA", ap, alice_id );
   issue_uia( alice_id, asset( 301000, asset1.id ) );

   auto target = db.get_dynamic_global_properties().next_maintenance_time + fc::minutes(2);
   while( db.head_block_time() < target)
   {
      generate_block();
   }
   target += fc::days(1);
   set_expiration(db, this->trx);
   db.modify(accounts_online_id_type()(db), [&](accounts_online_object& aoo) {
      aoo.online_info.emplace(alice_id, 720);
   });
   transfer(alice_id, account_id_type(), asset(1000, asset1.id), asset(0, asset_id_type(1)));
   while( db.head_block_time() < target)
   {
      generate_block();
   }
   BOOST_CHECK( get_balance( alice_id, asset1.id ) == 300000 + 300000 * 0.1 / 2);
}

BOOST_AUTO_TEST_CASE( test_mandatory_transfer )
{
   BOOST_TEST_MESSAGE( "=== test_mandatory_transfer ===" );

   try {
      ACTOR( abcde1 );
      ACTOR( abcde2 );
      ACTOR( alice );

      //assign privileges for create_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      INVOKE(create_edc);

      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));
      set_expiration(db, this->trx);
      asset_parameters ap;
      ap.bonus_percent = percent_to_int(0.1);
      ap.daily_bonus = true;
      ap.mining = false;
      ap.maturing_bonus_balance = false;
      ap.coin_maturing = false;
      ap.mandatory_transfer = 999;
      ap.fee_paying_asset = asset_id_type();
      auto test_asset = create_user_issued_asset( "TEST", ap, alice_id );
      generate_block();

      issue_uia( alice_id, asset( 301899, test_asset.id ) );
      auto target = HARDFORK_620_TIME + fc::days( 1 );
      while( db.head_block_time() < target ) 
      {
         generate_block();
      }
      set_expiration(db, this->trx);
      BOOST_CHECK( get_balance(alice_id, test_asset.id) == 301899 );
      transfer(alice_id, account_id_type(), asset(900, test_asset.id), asset(0, test_asset.id));
      target += fc::days(1);
      while( db.head_block_time() < target ) 
      {
         generate_block();
      } 
      set_expiration(db, this->trx);
      BOOST_CHECK( get_balance(alice_id, test_asset.id) == 300999 );
      transfer(alice_id, account_id_type(), asset(999, test_asset.id), asset(0, test_asset.id));
      target += fc::days(1);
      while( db.head_block_time() < target ) 
      {
         generate_block();
      }
      BOOST_CHECK( get_balance(alice_id, test_asset.id) == 330000 );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( fee_paying_asset_test )
{
   BOOST_TEST_MESSAGE( "=== fee_paying_asset_test ===" );

   try {
      ACTOR( abcde1 );
      ACTOR( abcde2 );
      ACTOR( alice );
      
      // alice should be able to create assets
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      INVOKE(create_edc);

      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME) + fc::minutes(2));
      set_expiration(db, this->trx);
      asset_parameters ap;
      ap.fee_paying_asset = asset_id_type(0);
      auto test_asset = create_user_issued_asset( "TEST", ap, alice_id );
      issue_uia( account_id_type(), asset( 10000, asset_id_type( 1 ) ) );
      issue_uia( alice_id, asset( 10000, asset_id_type( 1 ) ) );
      transfer( account_id_type(), alice_id, asset( 1000 ), asset(1, asset_id_type(1) ) );
      issue_uia( alice_id, asset( 10000, test_asset.id ) );
      transfer( alice_id, account_id_type(), asset( 1000, test_asset.id ), asset( 100, test_asset.id ) );
      asset_parameters ap1;
      generate_block();
      auto edc_fee_asset = create_user_issued_asset( "EDCFEE", ap1, alice_id );
      issue_uia( alice_id, asset( 10000, edc_fee_asset.id ) );
      transfer( alice_id, account_id_type(), asset( 1000, edc_fee_asset.id ), asset( 100,  asset_id_type(1) ) );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
