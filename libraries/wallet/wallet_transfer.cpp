#include "wallet_api_impl.hpp"
#include <graphene/wallet/wallet.hpp>
#include <graphene/chain/settings_object.hpp>

/***
 * Methods to handle transfers / exchange orders
 */

namespace graphene { namespace wallet { namespace detail {

   signed_transaction wallet_api_impl::withdraw_vesting(
      string witness_name,
      string amount,
      string asset_symbol,
      bool broadcast)
   { try {
      asset_object asset_obj = get_asset( asset_symbol );
      fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>(witness_name);
      if( !vbid )
      {
         witness_object wit = get_witness( witness_name );
         FC_ASSERT( wit.pay_vb );
         vbid = wit.pay_vb;
      }

      vesting_balance_object vbo = get_object( *vbid );
      vesting_balance_withdraw_operation vesting_balance_withdraw_op;

      vesting_balance_withdraw_op.vesting_balance = *vbid;
      vesting_balance_withdraw_op.owner = vbo.owner;
      vesting_balance_withdraw_op.amount = asset_obj.amount_from_string(amount);

      signed_transaction tx;
      tx.operations.push_back( vesting_balance_withdraw_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (witness_name)(amount) ) }
   
   signed_transaction wallet_api_impl::sell_asset(string seller_account,
      string amount_to_sell,
      string symbol_to_sell,
      string min_to_receive,
      string symbol_to_receive,
      uint32_t timeout_sec,
      bool fill_or_kill,
      bool broadcast)
   {
      account_object seller   = get_account( seller_account );

      limit_order_create_operation op;
      op.seller = seller.id;
      op.amount_to_sell = get_asset(symbol_to_sell).amount_from_string(amount_to_sell);
      op.min_to_receive = get_asset(symbol_to_receive).amount_from_string(min_to_receive);
      if( timeout_sec )
         op.expiration = fc::time_point::now() + fc::seconds(timeout_sec);
      op.fill_or_kill = fill_or_kill;

      signed_transaction tx;
      tx.operations.push_back(op);
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   }
   
   signed_transaction wallet_api_impl::borrow_asset(string seller_name,
      string amount_to_borrow, string asset_symbol,
      string amount_of_collateral, bool broadcast)
   {
      account_object seller = get_account(seller_name);
      asset_object mia = get_asset(asset_symbol);
      FC_ASSERT(mia.is_market_issued());
      asset_object collateral = get_asset(get_object(*mia.bitasset_data_id).options.short_backing_asset);

      call_order_update_operation op;
      op.funding_account = seller.id;
      op.delta_debt   = mia.amount_from_string(amount_to_borrow);
      op.delta_collateral = collateral.amount_from_string(amount_of_collateral);

      signed_transaction trx;
      trx.operations = {op};
      set_operation_fees( trx, _remote_db->get_global_properties().parameters.get_current_fees());
      trx.validate();
      idump((broadcast));

      return sign_transaction(trx, broadcast);
   }
   
   signed_transaction wallet_api_impl::cancel_order(limit_order_id_type order_id, bool broadcast)
   { try {
         FC_ASSERT(!is_locked());
         signed_transaction trx;

         limit_order_cancel_operation op;
         op.fee_paying_account = get_object(order_id).seller;
         op.order = order_id;
         trx.operations = {op};
         set_operation_fees( trx, _remote_db->get_global_properties().parameters.get_current_fees());

         trx.validate();
         return sign_transaction(trx, broadcast);
   } FC_CAPTURE_AND_RETHROW((order_id)) }
   
   signed_transaction wallet_api_impl::transfer(string from,
      string to, string amount,
      string asset_symbol, string memo, bool broadcast)
   { try {
      FC_ASSERT( !self.is_locked() );
      fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
      FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_symbol));
      fc::optional<asset_object> fee_asset_obj = get_asset(asset_obj->params.fee_paying_asset);
      FC_ASSERT(fee_asset_obj, "Could not find fee paying asset matching ${asset}", ("asset", asset_obj->params.fee_paying_asset));

      account_object to_account;
      fc::optional<address> to_address;
      if (std::regex_match(to, graphene::chain::ADDRESS_REGEX))
      {
         to_address = address(to);
         auto accounts = _remote_db->get_address_references({*to_address})[0];

         // user's address
         if (accounts.size() == 1)
         {
            to_account = *_remote_db->get_accounts({accounts[0]})[0];
            to = to_account.name;
         }
         // market's address
         else
         {
            fc::optional<account_id_type> market_account_id = _remote_db->get_market_reference(address(to));
            FC_ASSERT(market_account_id);
            const vector<fc::optional<account_object>>& v = _remote_db->get_accounts({*market_account_id});
            FC_ASSERT(v.size() == 1);
            FC_ASSERT(v[0]);

            to_account = *v[0];
         }
         to = to_account.name;
      }
      else {
         to_account = get_account(to);
      }

      account_object from_account = get_account(from);
      account_id_type from_id = from_account.id;
      account_id_type to_id = get_account_id(to);

      transfer_operation xfer_op;
      xfer_op.from = from_id;
      xfer_op.to = to_id;
      xfer_op.amount = asset_obj->amount_from_string(amount);

      if( memo.size() )
      {
         xfer_op.memo = memo_data();
         xfer_op.memo->from = from_account.options.memo_key;
         xfer_op.memo->to = to_account.options.memo_key;
         xfer_op.memo->set_message( get_private_key(from_account.options.memo_key),
                                    to_account.options.memo_key, memo );
      }

      if (to_address) {
         xfer_op.extensions.emplace(string(*to_address));
      }

      signed_transaction tx;
      //      const dynamic_global_property_object& dynamic_props = get_dynamic_global_properties();
      bool custom_fee_enabled = false;
//      if (dynamic_props.time > HARDFORK_627_TIME)
//      {
      const settings_object& settings = get_object(settings_id_type(0));
      auto itr = std::find_if(settings.transfer_fees.begin(), settings.transfer_fees.end(), [&](const chain::settings_fee& f) {
         return (f.asset_id == asset_obj->params.fee_paying_asset);
      });
      if (itr != settings.transfer_fees.end())
      {
         custom_fee_enabled = true;
         share_type amount = std::round(xfer_op.amount.amount.value * (itr->percent / 100000.0));
         const asset& calc_fee = _remote_db->get_current_fee_schedule().calculate_fee(xfer_op, fee_asset_obj->options.core_exchange_rate);
         amount = (amount < calc_fee.amount) ? calc_fee.amount : amount;
         xfer_op.fee = asset(amount, asset_obj->params.fee_paying_asset);
      }
      //}
      tx.operations.push_back(xfer_op);

      if (!custom_fee_enabled) {
         set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), fee_asset_obj->options.core_exchange_rate );
      }

      tx.validate();

      _last_wallet_transfer.set(asset_symbol, from, to, amount, memo);

      return sign_transaction(tx, broadcast);

   } FC_CAPTURE_AND_RETHROW( (from)(to)(amount)(asset_symbol)(memo)(broadcast) ) }
    
}}} // namespace graphene::wallet::detail