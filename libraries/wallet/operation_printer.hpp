#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/operations.hpp>

#include <string>
#include <ostream>

#include "wallet_api_impl.hpp"

#define BRAIN_KEY_WORD_COUNT 16

namespace graphene { namespace wallet { namespace detail {
    
struct operation_result_printer
{
public:
   operation_result_printer( const wallet_api_impl& w )
      : _wallet(w) {}
   const wallet_api_impl& _wallet;
   typedef std::string result_type;

   std::string operator()(const void_result& x) const;
   std::string operator()(const object_id_type& oid);
   std::string operator()(const asset& a);
   std::string operator()(const string& s);
   std::string operator()(const eval_fund_dep_apply_object& x) const;
   std::string operator()(const eval_fund_dep_apply_object_fixed& x) const;
   std::string operator()(const market_address& x) const;
   std::string operator()(const dep_update_info& x) const;
   std::string operator()(const market_addresses& x) const;
};

// BLOCK  TRX  OP  VOP
struct operation_printer
{
private:
   ostream& out;
   const wallet_api_impl& wallet;
   operation_result result;

   std::string fee(const asset& a) const;

public:
   operation_printer( ostream& out, const wallet_api_impl& wallet, const operation_result& r = operation_result() )
      : out(out),
        wallet(wallet),
        result(r) { }

   typedef std::string result_type;

   template<typename T>
   std::string operator()(const T& op)const
   {
      //balance_accumulator acc;
      //op.get_balance_delta( acc, result );
      auto a = wallet.get_asset( op.fee.asset_id );
      auto payer = wallet.get_account( op.fee_payer() );

      string op_name = fc::get_typename<T>::name();
      if( op_name.find_last_of(':') != string::npos )
         op_name.erase(0, op_name.find_last_of(':')+1);
      out << op_name <<" ";
      // out << "balance delta: " << fc::json::to_string(acc.balance) <<"   ";
      out << payer.name << " fee: " << a.amount_to_pretty_string( op.fee );
      operation_result_printer rprinter(wallet);
      std::string str_result = result.visit(rprinter);
      if( str_result != "" )
      {
         out << "   result: " << str_result;
      }
      return "";
   }

   std::string operator()(const transfer_operation& op)const;
   std::string operator()(const transfer_from_blind_operation& op)const;
   std::string operator()(const transfer_to_blind_operation& op)const;
   std::string operator()(const account_create_operation& op)const;
   std::string operator()(const account_update_operation& op)const;
   std::string operator()(const account_update_authorities_operation& op)const;
   std::string operator()(const asset_create_operation& op)const;
   std::string operator()(const blind_transfer2_operation& op)const;
   std::string operator()(const fund_payment_operation& op)const;
   std::string operator()(const fund_deposit_update_operation& op)const;
   std::string operator()(const fund_deposit_reduce_operation& op)const;
};    
    
}}} // namespace graphene::wallet::detail