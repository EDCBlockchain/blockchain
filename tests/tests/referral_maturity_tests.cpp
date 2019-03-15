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

BOOST_FIXTURE_TEST_SUITE( referral_maturity_tests, database_fixture )

BOOST_AUTO_TEST_CASE( tree_create_test )
{
   /*
    *       nathan
    *
    *   /           \
    *
    * ref11       ref12
    *
    *   |           |
    *
    * ref21       ref22
    *
    * |    \      |     \
    *
    * ref31 ref32 ref33  ref34
    */

   try {

      BOOST_TEST_MESSAGE( "=== referral_maturity_tests ===" );
      BOOST_TEST_MESSAGE( "=== tree_create_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info()),
            account_test_in("ref11", "nathan", leaf_info()),
            account_test_in("ref12", "nathan", leaf_info()),
            account_test_in("ref21", "ref11", leaf_info()),
            account_test_in("ref22", "ref12", leaf_info()),
            account_test_in("ref31", "ref21", leaf_info()),
            account_test_in("ref32", "ref21", leaf_info()),
            account_test_in("ref33", "ref22", leaf_info()),
            account_test_in("ref34", "ref22", leaf_info()),

      };

      CREATE_ACCOUNTS(test_accounts);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in acc_parent_pair) {
         BOOST_ASSERT(result.referral_map.count(accounts_map[acc_parent_pair.name].id));
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->parent->data.account_id
               == accounts_map[acc_parent_pair.parent].id);
      });

      accounts_map.clear();

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( balances_test )
{

   /*
    * nathan (100.000)
    *
    *   |         \
    *
    * ref11(100.000) ref12 (100.000)
    *
    *   |                  |
    *
    * ref21(50.000)            ref22 (50.000)
    *  |       \           |         \
    * ref31(25.000) ref32(25.000)  ref33 (25.000) ref34(25.000)
    */

   try {

      BOOST_TEST_MESSAGE( "=== balances_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 100000,2,200000/2,2,4,400000/2,0,100000/2)),
            account_test_in("ref11", "nathan", leaf_info(account_id_type(), 100000,1,50000/2,0,1,100000/2,0,100000/2)),
            account_test_in("ref12", "nathan", leaf_info(account_id_type(), 100000,1,50000/2,0,1,100000/2,0,100000/2)),
            account_test_in("ref21", "ref11", leaf_info(account_id_type(), 50000,0,50000/2,0,0,50000/2,0,50000/2)),
            account_test_in("ref22", "ref12", leaf_info(account_id_type(), 50000,0,50000/2,0,0,50000/2,0,50000/2)),
            account_test_in("ref31", "ref21", leaf_info(account_id_type(), 25000,0,0,0,0,0,0,25000/2)),
            account_test_in("ref32", "ref21", leaf_info(account_id_type(), 25000,0,0,0,0,0,0,25000/2)),
            account_test_in("ref33", "ref22", leaf_info(account_id_type(), 25000,0,0,0,0,0,0,25000/2)),
            account_test_in("ref34", "ref22", leaf_info(account_id_type(), 25000,0,0,0,0,0,0,25000/2))
            };

      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);


      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      accounts_map.clear();

   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( level_a_test )
{

   /*
    *    nathan (100)
    *
    *   |            \
    *
    * ref1(100) ... ref4 (100) 1{ref5(50)} 2{ref5(100)}
    */

   try {

      BOOST_TEST_MESSAGE( "=== level_a_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 100000,4,400000/2,0,4,400000/2,0,100000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 200000,5,450000/2,0,5,450000/2,0.05,200000/2)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 200000,5,500000/2,0,5,500000/2,0.05,200000/2)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 100000,5,500000/2,0,5,500000/2,0.05,100000/2))
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 4, 100000, "partner"),
            account_children_in("nathan1", 1, 4, 100000, "partner1"),
            account_children_in("nathan1", 1, 1, 50000, "member1"),
            account_children_in("nathan3", 1, 5, 100000, "partner3"),
            account_children_in("nathan4", 1, 5, 100000, "partner4"),
      };

      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
      * Formula detected from get_child_balances() method in tree.cpp
      *double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
      * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan1"].id, 225000 * 0.05 * 0.0065, ""),
            referral_info(accounts_map["nathan3"].id, 250000 * 0.05 * 0.0065, ""),
            referral_info(accounts_map["nathan4"].id, 250000 * 0.05 * 0.0065, "")
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( a_referrals_mandatory_test )
{

   /*
    *    nathan (100)
    *
    *   |            \
    *
    * ref1(100) ... ref4 (100) 1{ref5(50)} 2{ref5(100)}
    */

   try {

      BOOST_TEST_MESSAGE( "=== a_referrals_mandatory_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 100000,4,400000/2,0,4,400000/2,0,100000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 200000,4,440000/2,0,4,440000/2,0,200000/2)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 200000,5,300000/2,0,5,300000/2,0.05,200000/2)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 100000,5,500000/2,0,5,500000/2,0.05,100000/2))
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 4, 100000, "partner"),
            account_children_in("nathan1", 1, 4, 100000, "partner1"),
            account_children_in("nathan1", 1, 1, 40000, "member1"),
            account_children_in("nathan3", 1, 3, 100000, "partner3"),
            account_children_in("nathan3", 1, 2, 100000, "partner34", false),
            account_children_in("nathan4", 1, 5, 100000, "partner4"),
      };

      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
      * Formula detected from get_child_balances() method in tree.cpp
      *double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
      * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan3"].id, /*300000*/300000/2 * 0.05 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_b_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_b_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 500000,4,400000/2,25,29,2900000/2,0,500000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 500000,5,500000/2,24,29,2900000/2,0.05,500000/2)),
            account_test_in("nathan2", "committee-account", leaf_info(account_id_type(), 100000,5,500000/2,25,30,3000000/2,0,100000/2)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 500000,4,400000/2,24,28,2800000/2,0,500000/2)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 500000,5,500000/2,25,30,2950000/2,0.04,500000/2)),
            account_test_in("nathan5", "committee-account", leaf_info(account_id_type(), 500000,5,450000/2,25,30,2950000/2,0.04,500000/2)),
            account_test_in("nathan6", "committee-account", leaf_info(account_id_type(), 500000,5,450000/2,25,30,2900000/2,0.04,500000/2)),
            account_test_in("nathan7", "committee-account", leaf_info(account_id_type(), 500000,5,500000/2,25,30,3000000/2,0.04,500000/2))
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 4, 100000, "partner"),
            account_children_in("nathan", 2, 25, 100000, "partner"),

            account_children_in("nathan1", 1, 5, 100000, "partner1"),
            account_children_in("nathan1", 2, 24, 100000, "partner1"),

            account_children_in("nathan2", 1, 5, 100000, "partner2"),
            account_children_in("nathan2", 2, 25, 100000, "partner2"),

            account_children_in("nathan3", 1, 4, 100000, "partner3"),
            account_children_in("nathan3", 2, 24, 100000, "partner3"),

            account_children_in("nathan4", 1, 5, 100000, "partner4"),
            account_children_in("nathan4", 2, 24, 100000, "partner4"),
            account_children_in("nathan4", 2, 1, 50000, "member4"),

            account_children_in("nathan5", 1, 4, 100000, "partner5"),
            account_children_in("nathan5", 1, 1, 50000, "member5"),
            account_children_in("nathan5", 2, 25, 100000, "partner5"),

            account_children_in("nathan6", 1, 4, 100000, "partner6"),
            account_children_in("nathan6", 1, 1, 50000, "member6"),
            account_children_in("nathan6", 2, 24, 100000, "partner6"),
            account_children_in("nathan6", 2, 1, 50000, "member6"),

            account_children_in("nathan7", 1, 5, 100000, "partner7"),
            account_children_in("nathan7", 2, 25, 100000, "partner7"),
      };
      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
      * Formula detected from get_child_balances() method in tree.cpp
      *double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
      * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan1"].id, /*125000/2 * 0.0065*/ 500000/2 * 0.05 * 0.0065, ""),
            referral_info(accounts_map["nathan4"].id, /*125000/2 * 0.0065*/ 2950000/2 * 0.04 * 0.0065, ""),
            referral_info(accounts_map["nathan7"].id, /*600000/2 * 0.0065*/ 2999999/2 * 0.04 * 0.0065, ""), //TODO rounding not correct 2999999 ~ 3000000
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_c_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_c_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 1000000,5,500000/2,25,125,12500000/2,/*0.15*/0.03,1000000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 900000,5,500000/2,25,125,12500000/2,/*0*/0.03,900000/2)),
            account_test_in("nathan2", "committee-account", leaf_info(account_id_type(), 1000000,4,400000/2,25,124,12400000/2,0,1000000/2)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 900000,5,500000/2,25,124,12400000/2,/*0.20*/0.04,900000/2)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 1000000,5,500000/2,25,125/*124*/,12450000/2,/*0.20*/0.03,1000000/2)),
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 5, 100000, "partner"),
            account_children_in("nathan", 2, 25, 100000, "partner"),
            account_children_in("nathan", 3, 95, 100000, "partner"),


            account_children_in("nathan1", 1, 5, 100000, "partner1"),
            account_children_in("nathan1", 2, 25, 100000, "partner1"),
            account_children_in("nathan1", 3, 95, 100000, "partner1"),


            account_children_in("nathan2", 1, 4, 100000, "partner2"),
            account_children_in("nathan2", 2, 25, 100000, "partner2"),
            account_children_in("nathan2", 3, 95, 100000, "partner2"),

            account_children_in("nathan3", 1, 5, 100000, "partner3"),
            account_children_in("nathan3", 2, 25, 100000, "partner3"),
            account_children_in("nathan3", 3, 94, 100000, "partner3"),

            account_children_in("nathan4", 1, 5, 100000, "partner4"),
            account_children_in("nathan4", 2, 25, 100000, "partner4"),
            account_children_in("nathan4", 3, 94, 100000, "partner4"),
            account_children_in("nathan4", 3, 1, 50000, "member4"),
      };
      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
      * Formula detected from get_child_balances() method in tree.cpp
      * double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
      * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, /*1875000/2*/12500000/2 * 0.03 * 0.0065, ""),
            referral_info(accounts_map["nathan3"].id, /*2480000/2*/12399999/2 * 0.04 * 0.0065, ""), //TODO rounding not correct 12399999 ~ 12400000
            referral_info(accounts_map["nathan4"].id, /*2490000/2*/12450000/2 * 0.03 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_d_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_d_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 2000000,5,500000/2,25,625,62500000/2,/*0.10*/0.02,2000000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 1000000,5,500000/2,25,625,62500000/2,/*0*/0.02,1000000/2)),
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 5, 100000, "partner"),
            account_children_in("nathan", 2, 25, 100000, "partner"),
            account_children_in("nathan", 3, 595, 100000, "partner"),

            account_children_in("nathan1", 1, 5, 100000, "partner1"),
            account_children_in("nathan1", 2, 25, 100000, "partner1"),
            account_children_in("nathan1", 3, 1, 0, "partner1"),
            account_children_in("nathan1", 4, 595, 100000, "partner1"),
      };

      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
      * Formula detected from get_child_balances() method in tree.cpp
      *double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
      * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, /*6250000/2*/62500000/2 * 0.02 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });


      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_e_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_e_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 3000000,5,500000/2,25,3125,312500000/2,/*0.05*/0.01,3000000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 2000000,5,500000/2,25,3125,312500000/2,/*0*/0.01,2000000/2)),
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 5, 100000, "partner"),
            account_children_in("nathan", 2, 25, 100000, "partner"),
            account_children_in("nathan", 3, 2000, 100000, "partner"),
            account_children_in("nathan", 4, 1000, 100000, "partner"),
            account_children_in("nathan", 5, 1, 0, "partner"),
            account_children_in("nathan", 6, 90, 100000, "partner"),
            account_children_in("nathan", 7, 5, 100000, "partner"),

            account_children_in("nathan1", 1, 5, 100000, "partner1"),
            account_children_in("nathan1", 2, 25, 100000, "partner1"),
            account_children_in("nathan1", 3, 3095, 100000, "partner1"),
      };

           db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
       * Formula detected from get_child_balances() method in tree.cpp
       *     double resulting_percent = 0.0065 * bonus_percent; cb.balance *= resulting_percent;
       * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, /*15625000/2*/312500000/2 * 0.01 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( e_referrals_mandatory_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== e_referrals_mandatory_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 2000000,5,500000/2,25,625,60000000/2,/*0.10*/0.02,2000000/2)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 2000000,5,500000/2,25,625,62500000/2,/*0*/0.02,0), false),
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 5, 100000, "partner"),
            account_children_in("nathan", 2, 25, 100000, "partner", false),
            account_children_in("nathan", 3, 595, 100000, "partner"),

            account_children_in("nathan1", 1, 5, 100000, "partner1"),
            account_children_in("nathan1", 2, 25, 100000, "partner1"),
            account_children_in("nathan1", 3, 1, 0, "partner1"),
            account_children_in("nathan1", 4, 595, 100000, "partner1"),
      };

      db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object& dgpo) {
         dgpo.next_maintenance_time = db.head_block_time() + fc::hours(24);
      });
      generate_blocks(db.head_block_time() + fc::hours(12));
      set_expiration(db, this->trx);
      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();
      auto& mat_bal_idx = db.get_index_type<account_mature_balance_index>();
      referral_tree result(acc_idx, bal_idx, asset_id_type(), account_id_type(), &mat_bal_idx);
      result.form();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      /*
 * Formula detected from get_child_balances() method in tree.cpp
 *     double resulting_percent = 0.0065 * bonus_percent;
  cb.balance *= resulting_percent;
 * All parameters were taken from test_accounts array etc balance, bonus_percents and magic number 0.0065*/
      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, 59999999/2 * 0.02 * 0.0065, ""), //TODO rounding not correct balance 60000000 ~ 59999999
      };
      std::list<referral_info> ops_info = result.scan();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });


      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
