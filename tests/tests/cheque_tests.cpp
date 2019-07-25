#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

#include <graphene/chain/cheque_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(cheque_tests, database_fixture)

BOOST_AUTO_TEST_CASE(create_and_use_cheque_test)
{

   try
   {
      BOOST_TEST_MESSAGE( "=== create_and_use_cheque_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);
      ACTOR(bob);
      ACTOR(dan);

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(alice_id, asset(10000, EDC_ASSET));
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);

      std::string cheque_code = "Ab34567890123456";
      std::string cheque_code_case_sens = "ab34567890123456";
      std::string cheque_code_bad = "=%!4567890123456";
      fc::time_point_sec exp_date = db.head_block_time() + fc::days(2);

      // check if code is bad...
      BOOST_CHECK_THROW(make_cheque(cheque_code_bad, exp_date, EDC_ASSET, 1000, 2, alice_id), fc::assert_exception);
      trx.clear();
      // make cheque from alice for 2 account per 1000
      make_cheque(cheque_code, exp_date, EDC_ASSET, 1000, 2, alice_id);

      // check exception if make check with a same code again
      BOOST_REQUIRE_THROW(make_cheque(cheque_code, exp_date, EDC_ASSET, 1000, 2, alice_id), fc::exception);
      trx.clear();

      auto cheque_iter = db.get_index_type<cheque_index>().indices().get<by_code>().find(cheque_code);
      BOOST_CHECK(cheque_iter != db.get_index_type<cheque_index>().indices().get<by_code>().end());
      const cheque_object& cheque = *cheque_iter;

      // check all fields for a cheque
      BOOST_CHECK(cheque.code == cheque_code);
      BOOST_CHECK(cheque.datetime_expiration == exp_date);

      share_type sum_amount = cheque.payees.size() * cheque.amount_payee;
      BOOST_CHECK(sum_amount == 1000 * 2);

      BOOST_CHECK(cheque.asset_id == EDC_ASSET);
      BOOST_CHECK(cheque.drawer == alice_id);

      // check alice balance after cheque creation
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 8000);

      // use cheque with invalid code must raise an exception
      BOOST_REQUIRE_THROW(use_cheque(cheque_code_bad, bob_id), fc::assert_exception);
      trx.clear();

      //use cheque case sensitivity, must throw exception that cheque not found
      BOOST_REQUIRE_THROW(use_cheque(cheque_code_case_sens, bob_id), fc::assert_exception);
      trx.clear();
      
      // use cheque for bob
      use_cheque(cheque_code, bob_id);

      // twice use cheque for a bob must throw an exception
      BOOST_REQUIRE_THROW(use_cheque(cheque_code, bob_id), fc::assert_exception);
      trx.clear();

      // use check for dan
      use_cheque(cheque_code, dan_id);

      // twice usage a single code after all items has been used must raise an exception
      BOOST_REQUIRE_THROW(use_cheque(cheque_code, dan_id), fc::assert_exception);
      trx.clear();

      // check if a cheque has been set status to used
      cheque_iter = db.get_index_type<cheque_index>().indices().get<by_code>().find(cheque_code);
      BOOST_CHECK(cheque_iter != db.get_index_type<cheque_index>().indices().get<by_code>().end());
      const cheque_object& cheque_alice = *cheque_iter;
      BOOST_CHECK_EQUAL(cheque_alice.status, cheque_status::cheque_used);

      //check bob balance after cheque usage
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 1000);

      //check bob balance after cheque usage
      BOOST_CHECK(get_balance(dan_id, EDC_ASSET) == 1000);

   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(undo_expired_cheques_test)
{
   try
   {
      BOOST_TEST_MESSAGE( "=== undo_expired_cheques_test ===" );

      ACTOR(abcde1); // for needed IDs
      ACTOR(abcde2);
      ACTOR(alice);
      ACTOR(bob);
      ACTOR(dan);
      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id);

      create_edc();

      issue_uia(alice_id, asset(10000, EDC_ASSET));
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);
      issue_uia(bob_id, asset(10000, EDC_ASSET));
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 10000);

      std::string alice_cheque_code = "alicecode1111111";
      std::string bob_cheque_code = "bobcode111111111";
      fc::time_point_sec exp_date = db.head_block_time() + fc::days(2);

      // make cheque from an alice
      make_cheque(alice_cheque_code, exp_date, EDC_ASSET, 1000, 1, alice_id);
      // make cheque from an alice
      make_cheque(bob_cheque_code, exp_date, EDC_ASSET, 1000, 5, bob_id);

      // check alice balance after cheque creation
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9000);

      // check bob balance after cheque creation
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 5000);

      fc::time_point_sec current_block_time = db.head_block_time();
      while (db.head_block_time() < current_block_time + fc::days(1)) {
         generate_block();
      }
      // balance must be the same, check expiration 3 days
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9000);
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 5000);

      // let's use some part of bob cheque for dan
      use_cheque(bob_cheque_code, dan_id);

      // check bob balance after cheque usage
      BOOST_CHECK(get_balance(dan_id, EDC_ASSET) == 1000);

      current_block_time = db.head_block_time();
      while (db.head_block_time() < current_block_time + fc::days(3)) {
         generate_block();
      }

      // amounts must returned from cheques to owner
      BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);

      //for bob returned 4000 because 1000 was used by dan
      BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 9000);

      // cheques must be changed status to 'cheque_undo'
      auto cheque_iter = db.get_index_type<cheque_index>().indices().get<by_code>().find(alice_cheque_code);
      BOOST_CHECK(cheque_iter != db.get_index_type<cheque_index>().indices().get<by_code>().end());
      const cheque_object& cheque_alice = *cheque_iter;
      BOOST_CHECK_EQUAL(cheque_alice.status, cheque_undo);

      cheque_iter = db.get_index_type<cheque_index>().indices().get<by_code>().find(bob_cheque_code);
      BOOST_CHECK(cheque_iter != db.get_index_type<cheque_index>().indices().get<by_code>().end());
      const cheque_object& cheque_bob = *cheque_iter;
      BOOST_CHECK_EQUAL(cheque_bob.status, cheque_undo);

   } FC_LOG_AND_RETHROW()

}


BOOST_AUTO_TEST_SUITE_END()