/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
