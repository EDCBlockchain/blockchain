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

   } catch (fc::exception& e) {
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

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_SUITE_END()
