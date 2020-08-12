#include "wallet_api_impl.hpp"
#include <graphene/wallet/wallet.hpp>

namespace graphene { namespace wallet { namespace detail {

   optional<asset_object> wallet_api_impl::find_asset(asset_id_type id)const
   {
      auto rec = _remote_db->get_assets({id}).front();
      if (rec) {
         _asset_cache[id] = *rec;
      }
      return rec;
   }

   optional<asset_object> wallet_api_impl::find_asset(string asset_symbol_or_id)const
   {
      FC_ASSERT( asset_symbol_or_id.size() > 0 );

      if (auto id = maybe_id<asset_id_type>(asset_symbol_or_id))
      {
         // It's an ID
         return find_asset(*id);
      }
      else
      {
         // It's a symbol
         auto rec = _remote_db->lookup_asset_symbols({asset_symbol_or_id}).front();
         if (rec)
         {
            if (rec->symbol != asset_symbol_or_id) {
               return fc::optional<asset_object>();
            }
            _asset_cache[rec->get_id()] = *rec;
         }
         return rec;
      }
   } 
   
   asset_object wallet_api_impl::get_asset(asset_id_type id)const
   {
      auto opt = find_asset(id);
      FC_ASSERT(opt);
      return *opt;
   }

   asset_object wallet_api_impl::get_asset(string asset_symbol_or_id)const
   {
      auto opt = find_asset(asset_symbol_or_id);
      FC_ASSERT(opt);
      return *opt;
   }

   asset_id_type wallet_api_impl::get_asset_id(string asset_symbol_or_id) const
   {
      FC_ASSERT( asset_symbol_or_id.size() > 0 );
      vector<fc::optional<asset_object>> opt_asset;
      if (std::isdigit( asset_symbol_or_id.front())) {
         return fc::variant(asset_symbol_or_id, 1).as<asset_id_type>( 1 );
      }
      opt_asset = _remote_db->lookup_asset_symbols({asset_symbol_or_id});
      FC_ASSERT( (opt_asset.size() > 0) && (opt_asset[0].valid()) );
      return opt_asset[0]->id;
   }
   
   signed_transaction wallet_api_impl::create_asset(string issuer,
      string symbol,
      uint8_t precision,
      asset_options common,
      fc::optional<bitasset_options> bitasset_opts,
      bool broadcast)
   { try {
     account_object issuer_account = get_account( issuer );
     FC_ASSERT(!find_asset(symbol).valid(), "Asset with that symbol already exists!");
     FC_ASSERT(issuer_account.can_create_and_update_asset, "This account can't create or update asset!");

     auto fees  = _remote_db->get_global_properties().parameters.current_fees;

     asset_create_operation create_op;
     create_op.issuer = issuer_account.id;
     create_op.symbol = symbol;
     create_op.precision = precision;
     create_op.common_options = common;
     create_op.bitasset_opts = bitasset_opts;
     //      create_op.fee = fees->calculate_fee( create_op, edc_asset.options.core_exchange_rate );
     signed_transaction tx;
     tx.operations.push_back( create_op );
     set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
     tx.validate();

     return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (issuer)(symbol)(precision)(common)(bitasset_opts)(broadcast) ) }
   
   signed_transaction wallet_api_impl::update_asset(string symbol,
      fc::optional<string> new_issuer,
      asset_options new_options,
      chain::asset_parameters new_params,
      bool broadcast)
   { try {
         fc::optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update) {
         FC_THROW("No asset with that symbol exists!");
      }

      account_object current_issuer = get_account(asset_to_update->issuer);
      if (!current_issuer.can_create_and_update_asset) {
         FC_THROW("Current asset issuer can't update asset (restricted by committee)!");
      }

      fc::optional<account_id_type> new_issuer_account_id = false;
      if (new_issuer)
      {
         account_object new_issuer_account = get_account(*new_issuer);
         new_issuer_account_id = new_issuer_account.id;
      }

      asset_update2_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_issuer = new_issuer_account_id;
      update_op.new_options = new_options;
      update_op.new_params = new_params;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_issuer)(new_options)(new_params)(broadcast) ) }
   
   signed_transaction wallet_api_impl::update_bitasset(string symbol,
      bitasset_options new_options,
      bool broadcast)
   { try {
      fc::optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_bitasset_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_options = new_options;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_options)(broadcast) ) }
   
   signed_transaction wallet_api_impl::update_asset_feed_producers(string symbol,
      flat_set<string> new_feed_producers,
      bool broadcast)
   { try {
      fc::optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_update_feed_producers_operation update_op;
      update_op.issuer = asset_to_update->issuer;
      update_op.asset_to_update = asset_to_update->id;
      update_op.new_feed_producers.reserve(new_feed_producers.size());
      std::transform(new_feed_producers.begin(), new_feed_producers.end(),
                     std::inserter(update_op.new_feed_producers, update_op.new_feed_producers.end()),
                     [this](const std::string& account_name_or_id){ return get_account_id(account_name_or_id); });

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(new_feed_producers)(broadcast) ) }
   
   signed_transaction wallet_api_impl::publish_asset_feed(string publishing_account,
      string symbol,
      price_feed feed,
      bool broadcast)
   { try {
      fc::optional<asset_object> asset_to_update = find_asset(symbol);
      if (!asset_to_update)
        FC_THROW("No asset with that symbol exists!");

      asset_publish_feed_operation publish_op;
      publish_op.publisher = get_account_id(publishing_account);
      publish_op.asset_id = asset_to_update->id;
      publish_op.feed = feed;

      signed_transaction tx;
      tx.operations.push_back( publish_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (publishing_account)(symbol)(feed)(broadcast) ) }
   
   signed_transaction wallet_api_impl::fund_asset_fee_pool(string from,
      string symbol,
      string amount,
      bool broadcast)
   { try {
      account_object from_account = get_account(from);
      fc::optional<asset_object> asset_to_fund = find_asset(symbol);
      if (!asset_to_fund)
        FC_THROW("No asset with that symbol exists!");
      asset_object core_asset = get_asset(asset_id_type());

      asset_fund_fee_pool_operation fund_op;
      fund_op.from_account = from_account.id;
      fund_op.asset_id = asset_to_fund->id;
      fund_op.amount = core_asset.amount_from_string(amount).amount;

      signed_transaction tx;
      tx.operations.push_back( fund_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees(), core_asset.options.core_exchange_rate );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (from)(symbol)(amount)(broadcast) ) }
   
   signed_transaction wallet_api_impl::edc_fund_asset_fee_pool(string from,
      string symbol,
      share_type amount,
      bool broadcast)
   { try {
      account_object from_account = get_account(from);
      fc::optional<asset_object> asset_to_fund = find_asset(symbol);
      if (!asset_to_fund)
        FC_THROW("No asset with that symbol exists!");
      asset_object edc_asset = get_asset(EDC_ASSET_SYMBOL);

      edc_asset_fund_fee_pool_operation fund_op;
      fund_op.from_account = from_account.id;
      fund_op.asset_id = asset_to_fund->id;
      fund_op.amount = amount;
      signed_transaction tx;
      tx.operations.push_back( fund_op );
      _remote_db->get_global_properties().parameters.current_fees->set_fee( tx.operations.front(), edc_asset.options.core_exchange_rate );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (from)(symbol)(amount)(broadcast) ) }
   
   signed_transaction wallet_api_impl::reserve_asset(string from,
      string amount,
      string symbol,
      bool broadcast)
   { try {
      account_object from_account = get_account(from);
      fc::optional<asset_object> asset_to_reserve = find_asset(symbol);
      if (!asset_to_reserve)
        FC_THROW("No asset with that symbol exists!");

      asset_reserve_operation reserve_op;
      reserve_op.payer = from_account.id;
      reserve_op.amount_to_reserve = asset_to_reserve->amount_from_string(amount);

      signed_transaction tx;
      tx.operations.push_back( reserve_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (from)(amount)(symbol)(broadcast) ) }
   
   signed_transaction wallet_api_impl::global_settle_asset(string symbol,
      price settle_price,
      bool broadcast)
   { try {
      fc::optional<asset_object> asset_to_settle = find_asset(symbol);
      if (!asset_to_settle)
        FC_THROW("No asset with that symbol exists!");

      asset_global_settle_operation settle_op;
      settle_op.issuer = asset_to_settle->issuer;
      settle_op.asset_to_settle = asset_to_settle->id;
      settle_op.settle_price = settle_price;

      signed_transaction tx;
      tx.operations.push_back( settle_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (symbol)(settle_price)(broadcast) ) }

   signed_transaction wallet_api_impl::settle_asset(string account_to_settle,
      string amount_to_settle,
      string symbol,
      bool broadcast)
   { try {
      fc::optional<asset_object> asset_to_settle = find_asset(symbol);
      if (!asset_to_settle)
        FC_THROW("No asset with that symbol exists!");

      asset_settle_operation settle_op;
      settle_op.account = get_account_id(account_to_settle);
      settle_op.amount = asset_to_settle->amount_from_string(amount_to_settle);

      signed_transaction tx;
      tx.operations.push_back( settle_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_settle)(amount_to_settle)(symbol)(broadcast) ) }

   signed_transaction wallet_api_impl::issue_asset(string to_account,
      string amount, string symbol,
      string memo, bool broadcast)
   {
      auto asset_obj = get_asset(symbol);

      account_object to = get_account(to_account);
      account_object issuer = get_account(asset_obj.issuer);

      asset_issue_operation issue_op;
      issue_op.issuer           = asset_obj.issuer;
      issue_op.asset_to_issue   = asset_obj.amount_from_string(amount);
      issue_op.issue_to_account = to.id;

      if( memo.size() )
      {
         issue_op.memo = memo_data();
         issue_op.memo->from = issuer.options.memo_key;
         issue_op.memo->to = to.options.memo_key;
         issue_op.memo->set_message(get_private_key(issuer.options.memo_key),
                                    to.options.memo_key, memo);
      }

      signed_transaction tx;
      tx.operations.push_back(issue_op);
      set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }
   
   signed_transaction wallet_api_impl::propose_update_assets_fee_payer(
    const string& initiator
    , const fc::flat_set<std::string>& assets_to_update
    , const std::string& fee_payer_asset
    , fc::time_point_sec expiration_time
    , bool broadcast)
   { try {

      fc::flat_set<asset_id_type> assets_tmp;
      for (const std::string& item: assets_to_update)
      {
         const asset_object& asset_obj = get_asset(item); // assert will be invoked here if asset not exits
         assets_tmp.insert(asset_obj.get_id());
      }

      const asset_object& fee_payer_asset_tmp = get_asset(fee_payer_asset);

      assets_update_fee_payer_operation op;
      op.assets_to_update = assets_tmp;
      op.fee_payer_asset = fee_payer_asset_tmp.get_id();

      proposal_create_operation prop_op;
      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back(op);

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(assets_to_update)(fee_payer_asset)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::propose_update_asset_exchange_rate(
    const string& initiator
    , const std::string& asset_to_update
    , const variant_object& core_exchange_rate
    , fc::time_point_sec expiration_time
    , bool broadcast)
   { try {

      const asset_object& asset_to_update_tmp = get_asset(asset_to_update);

      price core_exchange_rate_tmp;
      fc::reflector<price>::visit(
         fc::from_variant_visitor<price>(core_exchange_rate, core_exchange_rate_tmp, GRAPHENE_MAX_NESTED_OBJECTS)
      );

      asset_update_exchange_rate_operation op;
      op.asset_to_update = asset_to_update_tmp.get_id();
      op.core_exchange_rate = core_exchange_rate_tmp;

      proposal_create_operation prop_op;
      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back(op);

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   } FC_CAPTURE_AND_RETHROW( (initiator)(asset_to_update)(core_exchange_rate)(expiration_time)(broadcast) ) }
   
   signed_transaction wallet_api_impl::propose_denominate(
    const string& initiator
    , bool enabled
    , const std::string& asst
    , uint32_t coef
    , fc::time_point_sec expiration_time
    , bool broadcast)
   { try {

      const asset_object& asset_obj = get_asset(asst);

      denominate_operation op;
      op.enabled = enabled;
      op.asset_id = asset_obj.get_id();
      op.coef = coef;

      proposal_create_operation prop_op;
      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = _remote_db->get_global_properties().parameters.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account_id(initiator);
      prop_op.proposed_ops.emplace_back(op);

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      bool broadcast = true;

      return sign_transaction( tx, broadcast);

   } FC_CAPTURE_AND_RETHROW( (asst)(coef) ) }
   
   asset wallet_api_impl::get_burnt_asset(asset_id_type id) {
      return _remote_db->get_burnt_asset(id);
   }
    
}}} // namespace graphene::wallet::detail