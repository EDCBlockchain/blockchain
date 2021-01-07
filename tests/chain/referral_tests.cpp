#include <boost/test/unit_test.hpp>

#include <graphene/app/application.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/tree.hpp>
#include <graphene/chain/fund_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"
#include "../common/test_utils.hpp"

#include <iostream>
#include <string>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( referral_tests, database_fixture )

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

      BOOST_TEST_MESSAGE( "=== referral_tests ===" );
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
      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in acc_parent_pair) {
         BOOST_ASSERT(result.referral_map.count(accounts_map[acc_parent_pair.name].id));
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->parent->data.account_id
               == accounts_map[acc_parent_pair.parent].id);
      });

      accounts_map.clear();

   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
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
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 100000,2,200000,0,2,400000,0)),
            account_test_in("ref11", "nathan", leaf_info(account_id_type(), 100000,0,50000,0,0,100000,0)),
            account_test_in("ref12", "nathan", leaf_info(account_id_type(), 100000,0,50000,0,0,100000,0)),
            account_test_in("ref21", "ref11", leaf_info(account_id_type(), 50000,0,50000,0,0,50000,0)),
            account_test_in("ref22", "ref12", leaf_info(account_id_type(), 50000,0,50000,0,0,50000,0)),
            account_test_in("ref31", "ref21", leaf_info(account_id_type(), 25000,0,0,0,0,0,0)),
            account_test_in("ref32", "ref21", leaf_info(account_id_type(), 25000,0,0,0,0,0,0)),
            account_test_in("ref33", "ref22", leaf_info(account_id_type(), 25000,0,0,0,0,0,0)),
            account_test_in("ref34", "ref22", leaf_info(account_id_type(), 25000,0,0,0,0,0,0))
            };

      CREATE_ACCOUNTS(test_accounts);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      accounts_map.clear();

   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
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
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 100000,4,400000,0,4,400000,0)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 200000,4,450000,0,4,450000,0)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 200000,5,500000,0,5,500000,0.25)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 100000,5,500000,0,5,500000,0))
      };

      std::vector<account_children_in> test_accounts_children = {
            account_children_in("nathan", 1, 4, 100000, "partner"),
            account_children_in("nathan1", 1, 4, 100000, "partner1"),
            account_children_in("nathan1", 1, 1, 50000, "member1"),
            account_children_in("nathan3", 1, 5, 100000, "partner3"),
            account_children_in("nathan4", 1, 5, 100000, "partner4"),
      };

      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);


      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan3"].id, 125000 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan_old();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_b_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_b_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 500000,4,400000,25,29,2900000,0)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 500000,5,500000,24,29,2900000,0.25)),
            account_test_in("nathan2", "committee-account", leaf_info(account_id_type(), 100000,5,500000,25,30,3000000,0)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 500000,4,400000,24,28,2800000,0)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 500000,5,500000,24,29,2950000,0.25)),
            account_test_in("nathan5", "committee-account", leaf_info(account_id_type(), 500000,4,450000,25,29,2950000,0)),
            account_test_in("nathan6", "committee-account", leaf_info(account_id_type(), 500000,4,450000,24,28,2900000,0)),
            account_test_in("nathan7", "committee-account", leaf_info(account_id_type(), 500000,5,500000,25,30,3000000,0.2))
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

      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);


      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan1"].id, 125000 * 0.0065, ""),
            referral_info(accounts_map["nathan4"].id, 125000 * 0.0065, ""),
            referral_info(accounts_map["nathan7"].id, 600000 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan_old();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_c_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_c_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 1000000,5,500000,25,125,12500000,0.15)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 900000,5,500000,25,125,12500000,0)),
            account_test_in("nathan2", "committee-account", leaf_info(account_id_type(), 1000000,4,400000,25,124,12400000,0)),
            account_test_in("nathan3", "committee-account", leaf_info(account_id_type(), 900000,5,500000,25,124,12400000,0.20)),
            account_test_in("nathan4", "committee-account", leaf_info(account_id_type(), 1000000,5,500000,25,124,12450000,0.20)),
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

      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);


      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, 1875000 * 0.0065, ""),
            referral_info(accounts_map["nathan3"].id, 2480000 * 0.0065, ""),
            referral_info(accounts_map["nathan4"].id, 2490000 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan_old();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_d_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_d_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 2000000,5,500000,25,625,62500000,0.10)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 1000000,5,500000,25,625,62500000,0)),
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

      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);

      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      std::list<referral_info> ops_info_expected = {
         referral_info(accounts_map["nathan"].id, 6250000 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan_old();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();
   }
   catch(fc::exception& e)
   {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE( level_e_test )
{

   try {

      BOOST_TEST_MESSAGE( "=== level_e_test ===" );

      std::vector<account_test_in> test_accounts = {
            account_test_in("nathan", "committee-account", leaf_info(account_id_type(), 3000000,5,500000,25,3125,312500000,0.05)),
            account_test_in("nathan1", "committee-account", leaf_info(account_id_type(), 2000000,5,500000,25,3125,312500000,0)),
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

      CREATE_ACCOUNTS(test_accounts);

      APPEND_CHILDREN(test_accounts_children);


      auto& acc_idx = db.get_index_type<account_index>();
      auto& bal_idx = db.get_index_type<account_balance_index>();

      referral_tree result(acc_idx, bal_idx, asset_id_type());
      result.form_old();

      std::for_each( test_accounts.begin(), test_accounts.end(), [&](account_test_in& acc_parent_pair) {
         BOOST_CHECK(result.referral_map[accounts_map[acc_parent_pair.name].id].node->data == acc_parent_pair.exp_data);
      });

      std::list<referral_info> ops_info_expected = {
            referral_info(accounts_map["nathan"].id, 15625000 * 0.0065, ""),
      };
      std::list<referral_info> ops_info = result.scan_old();

      BOOST_CHECK(ops_info_expected.size() <= ops_info.size());

      std::for_each( ops_info_expected.begin(), ops_info_expected.end(), [&](referral_info& op_in) {
         auto it = std::find_if(ops_info.begin(), ops_info.end(), referral_info_finder(op_in));
         BOOST_CHECK(it != ops_info.end());
      });

      accounts_map.clear();


   } catch(fc::exception& e) {
      edump((e.to_detail_string()))
      throw;
   }
}

BOOST_AUTO_TEST_CASE( referrals_test )
{
   try
   {
      BOOST_TEST_MESSAGE( "=== referrals_test ===" );

      ACTOR(abcde1) // for needed IDs

      ACTOR(alice) // level 3 (id 1.2.17)

      ACTOR(test1) //-- id 1.2.18
      ACTOR(test2) // level 2
      ACTOR(test3) //--

      ACTOR(test4)  //-- id 1.2.21
      ACTOR(test5)  //
      ACTOR(test6)  //
      ACTOR(test7)  //
      ACTOR(test8)  // level 1
      ACTOR(test9)  //
      ACTOR(test10) //
      ACTOR(test11) //
      ACTOR(test12) //--

      ACTOR(test13) // id 1.2.30
      ACTOR(test14)
      ACTOR(test15)
      ACTOR(test16)
      ACTOR(test17)
      ACTOR(test18)
      ACTOR(test19)
      ACTOR(test20)
      ACTOR(test21)
      ACTOR(test22)
      ACTOR(test23)
      ACTOR(test24)
      ACTOR(test25)
      ACTOR(test26)
      ACTOR(test27)
      ACTOR(test28)
      ACTOR(test29)
      ACTOR(test30)
      ACTOR(test31)
      ACTOR(test32)
      ACTOR(test33)
      ACTOR(test34)
      ACTOR(test35)
      ACTOR(test36)
      ACTOR(test37)
      ACTOR(test38)
      ACTOR(test39)

      ACTOR(bob) // id 1.2.57

      ACTOR(test40)
      ACTOR(test41)
      ACTOR(test42)

      /**
       * initial tree
       *                                                                                                    alice
       *                                     /                                                                |                                                                   \
       *                                test1                                                               test2                                                                 test3
       *              /                   |                   \                          /                    |                    \                           /                    |                    \
       *          test4                 test5                 test6                  test7                  test8                  test9                  test10                  test11                 test12
       *     /     |     \          /     |     \         /     |      \         /     |      \         /     |      \         /     |      \         /     |      \         /      |      \        /      |      \
       * test13 test14 test15   test16 test17 test18   test19 test20 test21   test22 test23 test24   test25 test26 test27   test28 test29 test30   test31 test32 test33   test34 test35 test36   test37 test38 test39
       *
       */

      /********************** apply new referrers ********************/

      // line 1
      {
         CHANGE_REFERRER_MULTIPLE(("test1")("test2")("test3"), "alice")
         BOOST_CHECK(get_account_by_name("test3").referrer == get_account_by_name("alice").get_id());
      }

      // line 2
      {
         CHANGE_REFERRER_MULTIPLE(("test4")("test5")("test6"), "test1")
         BOOST_CHECK(get_account_by_name("test6").referrer == get_account_by_name("test1").get_id());

         CHANGE_REFERRER_MULTIPLE(("test7")("test8")("test9"), "test2")
         BOOST_CHECK(get_account_by_name("test9").referrer == get_account_by_name("test2").get_id());

         CHANGE_REFERRER_MULTIPLE(("test10")("test11")("test12"), "test3")
         BOOST_CHECK(get_account_by_name("test12").referrer == get_account_by_name("test3").get_id());
      }

      // line 3
      {
         CHANGE_REFERRER_MULTIPLE(("test13")("test14")("test15"), "test4")
         BOOST_CHECK(get_account_by_name("test15").referrer == get_account_by_name("test4").get_id());

         CHANGE_REFERRER_MULTIPLE(("test16")("test17")("test18"), "test5")
         BOOST_CHECK(get_account_by_name("test18").referrer == get_account_by_name("test5").get_id());

         CHANGE_REFERRER_MULTIPLE(("test19")("test20")("test21"), "test6")
         BOOST_CHECK(get_account_by_name("test21").referrer == get_account_by_name("test6").get_id());

         CHANGE_REFERRER_MULTIPLE(("test22")("test23")("test24"), "test7")
         BOOST_CHECK(get_account_by_name("test24").referrer == get_account_by_name("test7").get_id());

         CHANGE_REFERRER_MULTIPLE(("test25")("test26")("test27"), "test8")
         BOOST_CHECK(get_account_by_name("test27").referrer == get_account_by_name("test8").get_id());

         CHANGE_REFERRER_MULTIPLE(("test28")("test29")("test30"), "test9")
         BOOST_CHECK(get_account_by_name("test30").referrer == get_account_by_name("test9").get_id());

         CHANGE_REFERRER_MULTIPLE(("test31")("test32")("test33"), "test10")
         BOOST_CHECK(get_account_by_name("test33").referrer == get_account_by_name("test10").get_id());

         CHANGE_REFERRER_MULTIPLE(("test34")("test35")("test36"), "test11")
         BOOST_CHECK(get_account_by_name("test36").referrer == get_account_by_name("test11").get_id());

         CHANGE_REFERRER_MULTIPLE(("test37")("test38")("test39"), "test12")
         BOOST_CHECK(get_account_by_name("test39").referrer == get_account_by_name("test12").get_id());
      }

      /***************************************************************/

      // assign privileges for creating_asset_operation
      SET_ACTOR_CAN_CREATE_ASSET(alice_id)

      create_edc(10000000000, asset(100, CORE_ASSET), asset(1, EDC_ASSET));

      issue_uia(alice_id, asset(100000, EDC_ASSET));
      issue_uia(bob_id, asset(100000, EDC_ASSET));
      ISSUES_BY_NAMES(test, 1, 43, asset(100000, EDC_ASSET))
      BOOST_CHECK(get_balance(test42_id, EDC_ASSET) == 100000);

      fc::time_point_sec h_time = HARDFORK_638_TIME;
      generate_blocks(h_time);

      // create fund
      fund_options::fund_rate fr;
      fr.amount = 10000;
      fr.day_percent = 1000;
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

      // update referral settings
      {
         update_referral_settings_operation op;
         op.fee = asset();
         op.referral_payments_enabled = true;
         op.referral_level1_percent = 5000;
         op.referral_level2_percent = 4000;
         op.referral_level3_percent = 3000;
         op.referral_min_limit_edc_level1 = std::make_pair(10000, 10000);
         op.referral_min_limit_edc_level2 = std::make_pair(10000, 10000);
         op.referral_min_limit_edc_level3 = std::make_pair(10000, 10000);
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      /********** payments to fund *********/

      // alice
      {
         fund_deposit_operation op;
         op.amount = 10000;
         op.fee = asset();
         op.from_account = alice_id;
         op.period = 50;
         op.fund_id = fund.id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }
      // bob
      {
         fund_deposit_operation op;
         op.amount = 10000;
         op.fee = asset();
         op.from_account = bob_id;
         op.period = 50;
         op.fund_id = fund.id;
         set_expiration(db, trx);
         trx.operations.push_back(std::move(op));
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      MAKE_DEPOSIT_PAYMENTS_BY_NAMES(test, 1, 43, 10000)

      BOOST_CHECK(get_account_by_name("alice").edc_in_deposits == 10000);
      BOOST_CHECK(get_account_by_name("test42").edc_in_deposits_daily == 10000);

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_name("test3").referral_level == 2);
      BOOST_CHECK(get_account_by_name("alice").referral_level == 3);

      //std::cout << "test3.edc_in_deposits_daily: " << get_account_by_name("test3").edc_in_deposits_daily.value << std::endl;
      //std::cout << "test12 balance: " << get_balance(test12_id, EDC_ASSET) << std::endl;
      //std::cout << "test3 balance: " << get_balance(test3_id, EDC_ASSET) << std::endl;
      //std::cout << "test39 referrer: " << get_account_by_id(get_account_by_id(test39_id).referrer).name << std::endl;

      /**
       * 90000(old balance) + 40(fund_payment) + 1500(referral payment)
       * 1500 = 10000(daily_deposits of each referral) * 0.05(referral_level1_percent) * 3(referrals count of test12);
       */
      BOOST_CHECK(get_balance(test12_id, EDC_ASSET) == 91540);

      /**
       * 90000(old balance) + 40(fund_payment) + 5100(referral payment)
       *
       * 5100:
       * 500 * 3 (level 1 users: test10, test11, test12; leaf_info2::level1_payment field)
       * 400 * 9 (level 0 users: test31, test32, test33, test34, test35, test36, test37, test38, test39; leaf_info2::level2_payment field)
       */
      BOOST_CHECK(get_balance(test3_id, EDC_ASSET) == 95140);

      // change referral of test39
      CHANGE_REFERRER_MULTIPLE(("test39"), "abcde1")

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_name("test3").referral_level == 1);
      BOOST_CHECK(get_account_by_name("alice").referral_level == 2);

      CHANGE_REFERRER_MULTIPLE(("test39"), "test12")

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_name("test12").referral_level == 1);

      // bob
      CHANGE_REFERRER_MULTIPLE(("test37")("test38")("test39"), "bob")

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_id(get_account_by_name("test37").referrer).name == "bob");
      BOOST_CHECK(get_account_by_id(get_account_by_name("test38").referrer).name == "bob");
      BOOST_CHECK(get_account_by_id(get_account_by_name("test39").referrer).name == "bob");

      BOOST_CHECK(get_account_by_name("bob").referral_level == 1);

      // test12
      CHANGE_REFERRER_MULTIPLE(("test37")("test38")("test39"), "test12")

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_name("test12").referral_level == 1);
      BOOST_CHECK(get_account_by_name("test3").referral_level == 2);

      // add 0-level users to 2-level user (its level shouldn't be changed)
      CHANGE_REFERRER_MULTIPLE(("test40")("test41")("test42"), "test3")

      h_time = db.head_block_time() + fc::days(1);
      while (db.head_block_time() < h_time) {
         generate_block();
      }

      BOOST_CHECK(get_account_by_name("test3").referral_level == 2);

   }
   catch(fc::exception& e)
   {
      edump((e.to_detail_string()))
      throw;
   }
}


BOOST_AUTO_TEST_SUITE_END()
