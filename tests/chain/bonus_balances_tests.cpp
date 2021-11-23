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

BOOST_FIXTURE_TEST_SUITE( bonus_balances_tests, database_fixture )


BOOST_AUTO_TEST_CASE( create_edc )
{
   try {

      BOOST_TEST_MESSAGE( "=== create_edc ===" );

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

   }
}

BOOST_AUTO_TEST_CASE( test_bonus_balances )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_bonus_balances ===" );

      ACTOR(abcde1);
      ACTOR(abcde2);
      ACTOR(alice);
      INVOKE(create_edc);

      //const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("EDC");
      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find(EDC_ASSET_SYMBOL);
      issue_uia(alice_id, asset(300000, edc_asset.id));

      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME) + fc::minutes(10));

      auto& index = db.get_index_type<bonus_balances_index>().indices().get<by_account>();

      db.adjust_bonus_balance(alice_id, asset(10000, edc_asset.id));

      db.adjust_bonus_balance(alice_id, referral_balance_info(15000, "A"));

      auto bonus_balances_itr = index.find(alice_id);
      
      BOOST_CHECK( bonus_balances_itr->get_full_balance(edc_asset.id, db.head_block_time() - fc::hours(24)).amount.value == 0);
      BOOST_CHECK( bonus_balances_itr->get_full_balance(edc_asset.id, db.head_block_time()).amount.value == 25000);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
   
}

BOOST_AUTO_TEST_CASE( test_bonus_balances_in_different_days )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_bonus_balances_in_different_days ===" );

      ACTOR(abcde1);
      ACTOR(abcde2);
      ACTOR(alice);
      INVOKE(create_edc);
      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("EDC");
      issue_uia(alice_id, asset(300000, edc_asset.id));

      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME) + fc::minutes(10));

      auto& index = db.get_index_type<bonus_balances_index>().indices().get<by_account>();
      
      db.adjust_bonus_balance(alice_id, asset(10000, edc_asset.id));

      while(db.head_block_time() < HARDFORK_620_TIME + fc::hours(24)) 
      {
         generate_block();
      }

      db.adjust_bonus_balance(alice_id, asset(20000, edc_asset.id));

      while(db.head_block_time() < HARDFORK_620_TIME + fc::hours(48)) 
      {
         generate_block();
      }
      db.adjust_bonus_balance(alice_id, asset(30000, edc_asset.id));

      auto bonus_balances_itr = index.find(alice_id);
      BOOST_CHECK( bonus_balances_itr->get_full_balance(edc_asset.id, db.head_block_time() - fc::hours(48)).amount.value == 10000);
      BOOST_CHECK( bonus_balances_itr->get_full_balance(edc_asset.id, db.head_block_time() - fc::hours(24)).amount.value == 20000);
      BOOST_CHECK( bonus_balances_itr->get_full_balance(edc_asset.id, db.head_block_time()).amount.value == 30000);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
   
}

BOOST_AUTO_TEST_CASE( test_deleting )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_deleting ===" );

      ACTOR(abcde1);
      ACTOR(abcde2);
      ACTOR(alice);
      INVOKE(create_edc);
      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("EDC");
      issue_uia(alice_id, asset(300000, edc_asset.id));
    
      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));

      db.adjust_bonus_balance(alice_id, asset(10000, edc_asset.id));

      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 300000);
      auto saved_time = db.head_block_time();
      auto target = db.head_block_time() + fc::days(3);
      while(db.head_block_time() < target) 
      {
         generate_block();
      }
      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 310000);

      auto& index = db.get_index_type<bonus_balances_index>().indices().get<by_account>();
      auto bonus_balances_itr = index.find(alice_id);
      BOOST_CHECK(bonus_balances_itr->get_balances_by_date(saved_time).bonus_time == time_point_sec());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_not_releasing )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_not_releasing ===" );

      ACTOR(abcde1);
      ACTOR(abcde2);
      ACTOR(alice);
      INVOKE(create_edc);
      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("EDC");
      issue_uia(alice_id, asset(300000, edc_asset.id));
    
      generate_blocks(fc::time_point_sec(HARDFORK_620_TIME));

      db.adjust_bonus_balance(alice_id, asset(10000, edc_asset.id));

      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 300000);
      auto saved_time = db.head_block_time();
      (void)saved_time;

      auto target = db.head_block_time() + fc::days(2);
      while(db.head_block_time() < target) 
      {
         generate_block();
      }
      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value != 310000);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_bonus_balances_merging )
{
   try {

      BOOST_TEST_MESSAGE( "=== test_bonus_balances_merging ===" );

      ACTOR(abcde1);
      ACTOR(abcde2);
      ACTOR(alice);
      INVOKE(create_edc);
      const asset_object& edc_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("EDC");
      issue_uia(alice_id, asset(300000, edc_asset.id));

      //if set original HARDFORK_620_TIME value, then bonuses not adjusted after secondary call of adjust_bonus_balance()
      //because HARDFORK_620_FIX_TIME have another logic, and it is the next day after HARDFORK_620_TIME
      //but for issuing bonuses used for 3 days
      generate_blocks(fc::time_point_sec(HARDFORK_619_TIME));

      db.adjust_bonus_balance(alice_id, asset(10000, edc_asset.id));

      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 300000);
      auto target = db.head_block_time() + fc::days(3);
      while(db.head_block_time() < target) 
      {
         generate_block();
      }

      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 310000);

      db.adjust_bonus_balance(alice_id, asset(20000, edc_asset.id));

      target = db.head_block_time() + fc::days(3);
      while(db.head_block_time() < target) 
      {
         generate_block();
      }

      BOOST_CHECK(db.get_balance(alice_id, edc_asset.id).amount.value == 330000);

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_bonus_balances_object_before_after_hf620 )
{
   try {


   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
