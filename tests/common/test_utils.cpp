#include "test_utils.hpp"

using namespace graphene;
using namespace graphene::chain;

std::map<string, account_object> accounts_map;

void create_test_account(account_test_in& new_account, database_fixture &db)
{
   account_object parent = db.get_account(new_account.parent);
   db.create_account(new_account.name, parent, parent, 80, db.generate_private_key(new_account.name).get_public_key());
   db.upgrade_to_lifetime_member(db.get_account(new_account.name));
   accounts_map.insert(std::pair<string, account_object>(new_account.name, db.get_account(new_account.name)));
   accounts_map.insert(std::pair<string, account_object>(new_account.parent, db.get_account(new_account.parent)));
   new_account.exp_data.account_id = accounts_map[new_account.name].id;
   if(new_account.exp_data.balance) {
         int64_t mand_sum = new_account.mand_transfer ? asset::scaled_precision(asset_id_type()(db.db).precision).value : 0;
      db.transfer(db.committee_account, accounts_map[new_account.name].id, asset(new_account.exp_data.balance + mand_sum, asset_id_type()));
      if (new_account.mand_transfer)
         db.transfer(accounts_map[new_account.name].id, db.committee_account, asset(mand_sum, asset_id_type()));
   }
}

void append_children(account_children_in& new_account, database_fixture &db)
{
   if (!accounts_map.count(new_account.parent))
      return;
   std::string parent = new_account.parent;
   for (int i = 1; i < new_account.level; ++i) {
      for(auto it = accounts_map.begin(); it != accounts_map.end(); it++)
      {
         if(it->second.referrer == accounts_map[parent].id)
         {
            parent = it->second.name;
            break;
         }
      }
      std::string tmp_parent = parent;
      for (int q = 0; q < i; q++)
      {
         tmp_parent = accounts_map[tmp_parent].referrer(db.db).name;
      }
      if (tmp_parent != new_account.parent)
         return;
   }

   std::string acc_prefix = new_account.name_prefix.append(std::to_string(new_account.level)).append("n");
   for(int i = 0; i < new_account.count; i++)
   {
      std::string acc_name(acc_prefix);
      acc_name.append(std::to_string(i));
      db.create_account(acc_name, db.get_account(parent), db.get_account(parent), 80, db.generate_private_key(acc_name).get_public_key());
      accounts_map.insert(std::pair<string, account_object>(acc_name, db.get_account(acc_name)));
      if(new_account.balance > 0) {
         int64_t mand_sum = new_account.mand_transfer ? asset::scaled_precision(asset_id_type()(db.db).precision).value : 0;
         db.transfer(db.committee_account, accounts_map[acc_name].id, asset(new_account.balance + mand_sum, asset_id_type()));
         if (new_account.mand_transfer)
            db.transfer(accounts_map[acc_name].id, db.committee_account, asset(mand_sum, asset_id_type()));
      }
   }
}



