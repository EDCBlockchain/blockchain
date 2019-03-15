#pragma once

#include <boost/test/unit_test.hpp>

#include <graphene/app/application.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/protocol/referral_classes.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

#include <iostream>
#include <string>

using namespace graphene;
using namespace graphene::chain;

struct account_test_in {
   std::string name;
   std::string parent;
   leaf_info exp_data;
   bool mand_transfer;

   account_test_in(std::string _name, std::string _parent, leaf_info _exp_data, bool _mand_transfer = true) : 
      name(_name), parent(_parent), exp_data(_exp_data), mand_transfer(_mand_transfer) {};

};

struct account_children_in {
   std::string parent;
   int level;
   int count;
   uint64_t balance;
   std::string name_prefix;
   bool mand_transfer;

   account_children_in(std::string _parent, int _level, int _count, uint64_t _balance, std::string _name_prefix  = "ref", bool _mand_transfer = true) :
      parent(_parent), level(_level), count(_count), balance(_balance), name_prefix(_name_prefix), mand_transfer(_mand_transfer) { };
};

struct referral_info_finder
{
   referral_info op_inf;

   referral_info_finder(referral_info in) : op_inf(in) {};

   bool operator () ( const referral_info& m ) const {
      return ( (m.quantity == op_inf.quantity) && (m.to_account_id == op_inf.to_account_id) );
   }
};


extern std::map<string, account_object> accounts_map;

#define CREATE_ACCOUNTS(accounts) std::for_each( accounts.begin(), accounts.end(), [&](account_test_in& new_account) {create_test_account(new_account, *this);})
#define APPEND_CHILDREN(accounts) std::for_each( accounts.begin(), accounts.end(), [&](account_children_in& new_account) {append_children(new_account, *this);})


void create_test_account(account_test_in& new_account, database_fixture &db);

void append_children(account_children_in& new_account, database_fixture &db);
