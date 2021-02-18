// see LICENSE.txt

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( serialization_raw_test )
{
   try {

      BOOST_TEST_MESSAGE( "=== serialization_raw_test ===" );

      make_account();
      transfer_operation op;
      op.from = account_id_type(1);
      op.to = account_id_type(2);
      op.amount = asset(100);
      trx.operations.push_back( op );
      auto packed = fc::raw::pack( trx );
      signed_transaction unpacked = fc::raw::unpack<signed_transaction>(packed);
      unpacked.validate();
      BOOST_CHECK( digest(trx) == digest(unpacked) );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE( serialization_json_test )
{
   try {

      BOOST_TEST_MESSAGE( "=== serialization_json_test ===" );

      make_account();
      transfer_operation op;
      op.from = account_id_type(1);
      op.to = account_id_type(2);
      op.amount = asset(100);
      trx.operations.push_back( op );
      fc::variant packed(trx, GRAPHENE_MAX_NESTED_OBJECTS);
      signed_transaction unpacked = packed.as<signed_transaction>( GRAPHENE_MAX_NESTED_OBJECTS );
      unpacked.validate();
      BOOST_CHECK( digest(trx) == digest(unpacked) );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( json_tests )
{
   try {

      BOOST_TEST_MESSAGE( "=== json_tests ===" );

   auto var = fc::json::variants_from_string( "10.6 " );
   var = fc::json::variants_from_string( "10.5" );
   } catch ( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( extension_serialization_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== extension_serialization_test ===" );

      buyback_account_options bbo;
      bbo.asset_to_buy = asset_id_type(1000);
      bbo.asset_to_buy_issuer = account_id_type(2000);
      bbo.markets.emplace( asset_id_type() );
      bbo.markets.emplace( asset_id_type(777) );
      account_create_operation create_op = make_account( "rex" );
      create_op.registrar = account_id_type(1234);
      create_op.extensions.value.buyback_options = bbo;

      auto packed = fc::raw::pack( create_op );
      account_create_operation unpacked = fc::raw::unpack<account_create_operation>(packed);

      ilog( "original: ${x}", ("x", create_op) );
      ilog( "unpacked: ${x}", ("x", unpacked) );
   }
   catch ( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
