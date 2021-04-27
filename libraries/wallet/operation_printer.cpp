#include "operation_printer.hpp"
#include <graphene/protocol/base.hpp>
#include "wallet_api_impl.hpp"
#include <graphene/utilities/key_conversion.hpp>

namespace graphene { namespace wallet { namespace detail {
    
std::string operation_printer::fee(const asset& a)const {
   out << "   (Fee: " << wallet.get_asset(a.asset_id).amount_to_pretty_string(a) << ")";
   return "";
}

std::string operation_printer::operator()(const transfer_from_blind_operation& op)const
{
   auto a = wallet.get_asset( op.fee.asset_id );
   auto receiver = wallet.get_account( op.to );

   out <<  receiver.name
   << " received " << a.amount_to_pretty_string( op.amount ) << " from blinded balance";
   return "";
}
std::string operation_printer::operator()(const transfer_to_blind_operation& op)const
{
   auto fa = wallet.get_asset( op.fee.asset_id );
   auto a = wallet.get_asset( op.amount.asset_id );
   auto sender = wallet.get_account( op.from );

   out <<  sender.name
   << " sent " << a.amount_to_pretty_string( op.amount ) << " to " << op.outputs.size() << " blinded balance" << (op.outputs.size()>1?"s":"")
   << " fee: " << fa.amount_to_pretty_string( op.fee );
   return "";
}
string operation_printer::operator()(const transfer_operation& op) const
{
   out << "Transfer " << wallet.get_asset(op.amount.asset_id).amount_to_pretty_string(op.amount)
       << " from " << wallet.get_account(op.from).name << " to " << wallet.get_account(op.to).name;
   std::string memo;
   if( op.memo )
   {
      if( wallet.is_locked() )
      {
         out << " -- Unlock wallet to see memo.";
      } else {
         try {
            FC_ASSERT(wallet._keys.count(op.memo->to) || wallet._keys.count(op.memo->from), "Memo is encrypted to a key ${to} or ${from} not in this wallet.", ("to", op.memo->to)("from",op.memo->from));
            if( wallet._keys.count(op.memo->to) ) {
               auto my_key = wif_to_key(wallet._keys.at(op.memo->to));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               memo = op.memo->get_message(*my_key, op.memo->from);
               out << " -- Memo: " << memo;
            } else {
               auto my_key = wif_to_key(wallet._keys.at(op.memo->from));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               memo = op.memo->get_message(*my_key, op.memo->to);
               out << " -- Memo: " << memo;
            }
         } catch (const fc::exception& e) {
            out << " -- could not decrypt memo";
            elog("Error when decrypting memo: ${e}", ("e", e.to_detail_string()));
         }
      }
   }
   fee(op.fee);
   return memo;
}

std::string operation_printer::operator()(const account_create_operation& op) const
{
   out << "Create Account '" << op.name << "'";
   return fee(op.fee);
}

std::string operation_printer::operator()(const account_update_operation& op) const
{
   out << "Update Account '" << wallet.get_account(op.account).name << "'";
   return fee(op.fee);
}

std::string operation_printer::operator()(const account_update_authorities_operation& op) const
{
   out << "Update Account Authorities '" << wallet.get_account(op.account).name << "'";
   return fee(op.fee);
}

std::string operation_printer::operator()(const asset_create_operation& op) const
{
   out << "Create ";
   if( op.bitasset_opts.valid() )
      out << "BitAsset ";
   else
      out << "User-Issue Asset ";
   out << "'" << op.symbol << "' with issuer " << wallet.get_account(op.issuer).name;
   return fee(op.fee);
}

std::string operation_printer::operator()(const blind_transfer2_operation& op) const
{
   out << "<hidden> ";
   return "";
}

std::string operation_printer::operator()(const fund_payment_operation& op) const
{
   out << "Fund payment, fund ID " << fc::to_string(op.fund_id.space_id) << "." << fc::to_string(op.fund_id.type_id) << "." << op.fund_id.instance.value
       << ", amount " << wallet.get_asset(op.asset_to_issue.asset_id).amount_to_pretty_string(op.asset_to_issue);

   return fee(op.fee);
}

std::string operation_printer::operator()(const fund_deposit_update_operation& op) const
{
   out << "Fund deposit update, deposit ID " << fc::to_string(op.deposit_id.space_id) << "." << fc::to_string(op.deposit_id.type_id) << "." << op.deposit_id.instance.value
       << ", percent '" << op.percent << "', reset '" << std::boolalpha << op.reset << "'";

   return fee(op.fee);
}

std::string operation_printer::operator()(const fund_deposit_reduce_operation& op) const
{
   out << "Fund deposit reduce, deposit ID=" << fc::to_string(op.deposit_id.space_id) << "." << fc::to_string(op.deposit_id.type_id) << "." << op.deposit_id.instance.value
       << ", amount reduced by '" << op.amount.value << "'";

   return fee(op.fee);
}

std::string operation_printer::operator()(const fund_deposit_acceptance_operation& op) const
{
   out << "Fund set acceptance of deposits, fund ID=" << fc::to_string(op.fund_id.space_id) << "." << fc::to_string(op.fund_id.type_id) << "." << op.fund_id.instance.value
       << ", status '" << std::boolalpha << op.enabled << "'";

   return fee(op.fee);
}

std::string operation_printer::operator()(const hard_enable_autorenewal_deposits_operation& op) const
{
   out << "Hard enable of deposits autorenewal, accounts count=" << fc::to_string(op.accounts.size())
       << ", enabled='" << std::boolalpha << op.enabled << "'";

   return fee(op.fee);
}

std::string operation_result_printer::operator()(const void_result& x) const
{
   return "";
}

std::string operation_result_printer::operator()(const object_id_type& oid)
{
   return std::string(oid);
}

std::string operation_result_printer::operator()(const asset& a)
{
   return _wallet.get_asset(a.asset_id).amount_to_pretty_string(a);
}

std::string operation_result_printer::operator()(const string& s)
{
   return s;
}

std::string operation_result_printer::operator()(const eval_fund_dep_apply_object& obj) const
{
   std::ostringstream os;
   os << "{\"id\":\"" << std::string(obj.id) << "\""
      << ",\"datetime_begin\":\"" << std::string(obj.datetime_begin) << "\""
      << ",\"datetime_end\":\"" << std::string(obj.datetime_end) << "\""
      << ",\"percent\":\"" << std::to_string(obj.percent) << "\"}";

   return os.str();
}

std::string operation_result_printer::operator()(const eval_fund_dep_apply_object_fixed& obj) const
{
   std::ostringstream os;
   os << "{\"id\":\"" << std::string(obj.id) << "\""
      << ",\"datetime_begin\":\"" << std::string(obj.datetime_begin) << "\""
      << ",\"datetime_end\":\"" << std::string(obj.datetime_end) << "\""
      << ",\"daily_payment\":\"" << _wallet.get_asset(obj.daily_payment.asset_id).amount_to_pretty_string(obj.daily_payment) << "\"}";

   return os.str();
}

std::string operation_result_printer::operator()(const market_address& obj) const
{
   std::ostringstream os;
   os << "{\"id\":\"" << std::string(obj.id) << "\""
      << ",\"addr\":\"" << std::string(obj.addr) << "\"}";

   return os.str();
}

std::string operation_result_printer::operator()(const dep_update_info& obj) const
{
   std::ostringstream os;
   os << "{\"percent\":\"" << obj.percent << "\""
      << ",\"daily_payment\":\"" << _wallet.get_asset(obj.daily_payment.asset_id).amount_to_pretty_string(obj.daily_payment) << "\"}";

   return os.str();
}

std::string operation_result_printer::operator()(const market_addresses& obj) const
{
   std::ostringstream os;

   os << "[";
   for (const std::pair<object_id_type, std::string>& item: obj.addresses)
   {
      os << "{\"id\":\"" << std::string(item.first) << "\""
         << ",\"address\":\"" << item.second << "\"},";
   }

   std::string str = os.str().substr(0, os.str().size() - 1);
   str += "]";

   return str;
}    
    
}}} // graphene::wallet::detail