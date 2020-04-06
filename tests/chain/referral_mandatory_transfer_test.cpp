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

#define MEASURE_TIME(operation, description) \
	   auto start = fc::time_point::now(); \
	   operation; \
	   std::cout << " "<< description <<" completed in "  \
                << (fc::time_point::now() - start).count() / 1000000.0 \
                << " seconds " <<  std::endl;



BOOST_FIXTURE_TEST_SUITE( referral_mandatory_transfer_test, database_fixture )

BOOST_AUTO_TEST_CASE( no_out_transaction_issue_test )
{
   try {

      BOOST_TEST_MESSAGE( "=== no_out_transaction_issue_test ===" );

      ACTOR(alice);

      transfer(committee_account, alice_id, asset(300000, asset_id_type()));

      generate_block();

      transaction_evaluation_state evalState(&db);
      referral_issue_operation ref_op;
      ref_op.issue_to_account = alice_id;
      ref_op.asset_to_issue = asset(1, asset_id_type());
      ref_op.issuer = account_id_type(3);

      BOOST_REQUIRE_THROW(db.apply_operation(evalState, ref_op), fc::exception);


   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( old_out_transaction_issue_test )
{
   try {

      BOOST_TEST_MESSAGE( "=== old_out_transaction_issue_test ===" );

      ACTOR(alice);

      transfer(committee_account, alice_id, asset(300000, asset_id_type()));

      transfer(alice_id, committee_account, asset(1000, asset_id_type()));

      generate_block();

      fc::time_point_sec target = db.head_block_time() + fc::hours(24);

      while(db.head_block_time() < target)
      {
         generate_block();
      }

      transaction_evaluation_state evalState(&db);
      referral_issue_operation ref_op;
      ref_op.issue_to_account = alice_id;
      ref_op.asset_to_issue = asset(1, asset_id_type());
      ref_op.issuer = account_id_type(3);


      MEASURE_TIME(
            BOOST_REQUIRE_THROW(db.apply_operation(evalState, ref_op), fc::exception),
            "apply operation"
      );



   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( recent_out_transaction_issue_test )
{
   try {

      BOOST_TEST_MESSAGE( "=== recent_out_transaction_issue_test ===" );

      ACTORS((alice) (bob));

      SET_ACTOR_CAN_CREATE_ASSET(bob_id);

      const asset_object& asset = create_user_issued_asset( "A1A", bob, override_authority );

      transaction_evaluation_state evalState(&db);
      referral_issue_operation ref_op;
      ref_op.issue_to_account = alice_id;
      ref_op.asset_to_issue = asset.amount(1);
      ref_op.issuer = bob_id;

      BOOST_REQUIRE_NO_THROW(db.apply_operation(evalState, ref_op));
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( appply_time_test_tx_each_block )
{
   BOOST_TEST_MESSAGE( "=== apply_time_test_tx_each_block ===" );

   ACTOR(alice);

   fc::time_point_sec target = db.head_block_time() + fc::hours(24);

   while(db.head_block_time() < target)
   {
      transfer(committee_account, alice_id, asset(1, asset_id_type()));
      generate_block();
   }

   transaction_evaluation_state evalState(&db);
   referral_issue_operation ref_op;
   ref_op.issue_to_account = alice_id;
   ref_op.asset_to_issue = asset(1, asset_id_type());
   ref_op.issuer = account_id_type(3);


   MEASURE_TIME(
         BOOST_REQUIRE_THROW(db.apply_operation(evalState, ref_op), fc::exception),
         "apply operation"
   );

}

BOOST_AUTO_TEST_SUITE_END()
