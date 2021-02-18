// see LICENSE.txt

#pragma once
#include <map>
#include <typeindex>
#include <typeinfo>

namespace graphene { namespace chain {

class account_object;
class asset_object;
class database;

namespace detail {

bool _is_authorized_asset(const database& d, const account_object& acct, const asset_object& asset_obj);

} // ns detail

enum directionality_type
{
   receiver = 0x2,
   payer = 0x4,
};

inline bool not_restricted_account(const database& d, const account_object& acct, uint8_t type)
{
   const auto& idx = d.get_index_type<restricted_account_index>().indices().get<by_acc_id>();

   auto itr = idx.find(acct.id);

   if (itr == idx.end()) {
      return true;
   }

   if (itr->restriction_type && (type & *itr->restriction_type)) {
      return false;
   }

   return true;
}

/**
 * @return true if the account is whitelisted and not blacklisted to transact in the provided asset; false
 * otherwise.
 */

inline bool is_authorized_asset(const database& d, const account_object& acct, const asset_object& asset_obj)
{
   bool fast_check = !(asset_obj.options.flags & white_list);
   fast_check &= !(acct.allowed_assets.valid());

   if( fast_check )
      return true;

   bool slow_check = detail::_is_authorized_asset( d, acct, asset_obj );
   return slow_check;
}

} }
