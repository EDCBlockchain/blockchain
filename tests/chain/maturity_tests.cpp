#include <boost/test/unit_test.hpp>

#include <graphene/app/application.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/tree.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

#include <iostream>
#include <string>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( maturity_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_edc )
{
   try {

      BOOST_TEST_MESSAGE( "=== maturity_tests - database_fixture - create_edc ===" );

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

   }
}

BOOST_AUTO_TEST_CASE( maturity_test )
{
   BOOST_TEST_MESSAGE( "=== maturity_test ===" );

   try {
      INVOKE(create_edc);
      std::vector<account_test_in> test_accounts = {
         account_test_in( "nathan", "committee-account", leaf_info() ),
         account_test_in( "test1", "committee-account", leaf_info() )
      };

      CREATE_ACCOUNTS( test_accounts );
      generate_block();
      // db.get_dynamic_global_properties().next_maintenance_time
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgpo ) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours( 24 );
      });
      const account_object& nathan = get_account_by_name( "nathan" );
      const account_object& test1 = get_account_by_name( "test1" );

      issue_uia( account_id_type(), asset( 3000000, asset_id_type( 1 ) ) );
      generate_blocks( db.head_block_time() + fc::hours( 6 ) );

      transfer( account_id_type(), nathan.get_id(), asset( 100000, asset_id_type( 1 ) ) );
      transfer( account_id_type(), test1.get_id(), asset( 100000, asset_id_type( 1 ) ) );
      generate_blocks( db.head_block_time() + fc::hours( 6 ) );

      transfer( account_id_type(), nathan.get_id(), asset( 100000, asset_id_type( 1 ) ) );
      transfer( account_id_type(), test1.get_id(), asset( 100000, asset_id_type( 1 ) ) );
      generate_blocks( db.head_block_time() + fc::hours( 6 ) );

      transfer( account_id_type(), nathan.get_id(), asset( 100000, asset_id_type( 1 ) ) );
      transfer( account_id_type(), test1.get_id(), asset( 100000, asset_id_type( 1 ) ) );

      transfer( nathan.get_id(), account_id_type(), asset( 150000, asset_id_type( 1 ) ) );
      transfer( test1.get_id(), account_id_type(), asset( 250000, asset_id_type( 1 ) ) );

      BOOST_CHECK( get_balance_for_bonus( nathan.get_id(), asset_id_type( 1 ) ) == 100000 );
      BOOST_CHECK( get_balance_for_bonus( test1.get_id(), asset_id_type( 1 ) ) == 37500 );

   } catch(fc::exception& e) {
      edump( ( e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
