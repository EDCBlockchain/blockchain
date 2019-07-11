#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

#include <graphene/chain/receipt_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( receipt_tests, database_fixture )

   BOOST_AUTO_TEST_CASE(create_and_use_receipt_test)
   {

      try
      {
         BOOST_TEST_MESSAGE( "=== create_and_use_receipt_test ===" );

         ACTOR(abcde1); // for needed IDs
         ACTOR(abcde2);
         ACTOR(alice);
         ACTOR(bob);

         // assign privileges for creating_asset_operation
         SET_ACTOR_CAN_CREATE_ASSET(alice_id);

         create_edc();

         issue_uia(alice_id, asset(10000, EDC_ASSET));
         BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);

         std::string receipt_code = "Ab34567890123456";
         std::string receipt_code_bad = "=%!4567890123456";
         fc::time_point_sec exp_date = db.head_block_time() + fc::days(2);

         //check if code is bad...
         BOOST_CHECK_THROW(make_receipt(receipt_code_bad, exp_date, EDC_ASSET, 1000, alice_id), fc::assert_exception);
         trx.clear();
         //make receipt from an alice
         make_receipt(receipt_code, exp_date, EDC_ASSET, 1000, alice_id);

         //check exception if make check with a same code again
         BOOST_REQUIRE_THROW(make_receipt(receipt_code, exp_date, EDC_ASSET, 1000, alice_id), fc::exception);
         trx.clear();

         auto receipt_iter = db.get_index_type<receipt_index>().indices().get<by_code>().find(receipt_code);
         BOOST_CHECK(receipt_iter != db.get_index_type<receipt_index>().indices().get<by_code>().end());
         const receipt_object& receipt = *receipt_iter;

         //check all fields for a receipt
         BOOST_CHECK(receipt.receipt_code == receipt_code);
         BOOST_CHECK(receipt.datetime_expiration == exp_date);
         BOOST_CHECK(receipt.amount.amount == 1000);
         BOOST_CHECK(receipt.amount.asset_id == EDC_ASSET);
         BOOST_CHECK(receipt.maker == alice_id);

         //check alice balance after receipt creation
         BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9000);

         //use receipt with invalid code must raise an exception
         BOOST_REQUIRE_THROW(use_receipt(receipt_code_bad, bob_id), fc::assert_exception);
         trx.clear();

         //use receipt for a bob
         use_receipt(receipt_code, bob_id);

         //twice usage a single code must raise an exception
         BOOST_REQUIRE_THROW(use_receipt(receipt_code, bob_id), fc::assert_exception);
         trx.clear();

         //check if a receipt has been deleted
         receipt_iter = db.get_index_type<receipt_index>().indices().get<by_code>().find(receipt_code);
         BOOST_CHECK(receipt_iter != db.get_index_type<receipt_index>().indices().get<by_code>().end());
         const receipt_object& receipt_alice = *receipt_iter;
         BOOST_CHECK_EQUAL(receipt_alice.receipt_status, receipt_used);

         //check bob balance after receipt usage
         BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 1000);


      }catch (fc::exception& e)
      {
         edump((e.to_detail_string()));
         throw;
      }
   }

   BOOST_AUTO_TEST_CASE(undo_expired_receipts_test)
   {

      try
      {
         BOOST_TEST_MESSAGE( "=== undo_expired_receipts_test ===" );

         ACTOR(abcde1); // for needed IDs
         ACTOR(abcde2);
         ACTOR(alice);
         ACTOR(bob);

         // assign privileges for creating_asset_operation
         SET_ACTOR_CAN_CREATE_ASSET(alice_id);

         create_edc();

        issue_uia(alice_id, asset(10000, EDC_ASSET));
         BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 10000);
         issue_uia(bob_id, asset(10000, EDC_ASSET));
         BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 10000);

                  std::string alice_receipt_code = "alicecode1111111";
         std::string bob_receipt_code = "bobcode111111111";
         fc::time_point_sec exp_date = db.head_block_time() + fc::days(2);

         //make receipt from an alice
         make_receipt(alice_receipt_code, exp_date, EDC_ASSET, 1000, alice_id);
         //make receipt from an alice
         make_receipt(bob_receipt_code, exp_date, EDC_ASSET, 1000, bob_id);

         //check alice balance after receipt creation
         BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9000);

         //check bob balance after receipt creation
         BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 9000);

         fc::time_point_sec current_block_time = db.head_block_time();
         while( db.head_block_time() < current_block_time + fc::days( 1 ) )
         {
            generate_block();
         }
         //balance must be the same, check expiration 3 days
         BOOST_CHECK(get_balance(alice_id, EDC_ASSET) == 9000);
         BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 9000);

         current_block_time = db.head_block_time();
         while( db.head_block_time() < current_block_time + fc::days( 3 ) )
         {
            generate_block();
         }

         //amounts must returned from receipts to owner
         int64_t b1 = get_balance(alice_id, EDC_ASSET);
         BOOST_CHECK(b1 == 10000);
         BOOST_CHECK(get_balance(bob_id, EDC_ASSET) == 10000);

         //receipts must be changed status to 'receipt_undo'
         auto receipt_iter = db.get_index_type<receipt_index>().indices().get<by_code>().find(alice_receipt_code);
         BOOST_CHECK(receipt_iter != db.get_index_type<receipt_index>().indices().get<by_code>().end());
         const receipt_object& receipt_alice = *receipt_iter;
         BOOST_CHECK_EQUAL(receipt_alice.receipt_status, receipt_undo);

         receipt_iter = db.get_index_type<receipt_index>().indices().get<by_code>().find(bob_receipt_code);
         BOOST_CHECK(receipt_iter != db.get_index_type<receipt_index>().indices().get<by_code>().end());
         const receipt_object& receipt_bob = *receipt_iter;
         BOOST_CHECK_EQUAL(receipt_bob.receipt_status, receipt_undo);

      } FC_LOG_AND_RETHROW()
   }


BOOST_AUTO_TEST_SUITE_END()