// see LICENSE.txt

#include <cctype>

#include <graphene/app/api.hpp>
#include <graphene/app/api_access.hpp>
#include <graphene/app/application.hpp>
#include <graphene/app/impacted.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/get_config.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/transaction_history_object.hpp>
#include <graphene/chain/withdraw_permission_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/fund_object.hpp>
#include <graphene/chain/witnesses_info_object.hpp>
#include <graphene/chain/cheque_object.hpp>
#include <graphene/chain/operation_history_object.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/crypto/base64.hpp>
#include <fc/rpc/api_connection.hpp>
#include <boost/range/adaptor/reversed.hpp>

template class fc::api<graphene::app::network_broadcast_api>;
template class fc::api<graphene::app::network_node_api>;
template class fc::api<graphene::app::history_api>;
template class fc::api<graphene::app::crypto_api>;
template class fc::api<graphene::app::database_api>;
template class fc::api<graphene::debug_witness::debug_api>;
template class fc::api<graphene::app::login_api>;

namespace graphene { namespace app {

    login_api::login_api(application& a)
    :_app(a) { }

    login_api::~login_api() { }

    bool login_api::login(const string& user, const string& password)
    {
       optional< api_access_info > acc = _app.get_api_access_info( user );
       if (!acc.valid()) {
          return false;
       }

       if (acc->password_hash_b64 != "*")
       {
          std::string password_salt = fc::base64_decode( acc->password_salt_b64 );
          std::string acc_password_hash = fc::base64_decode( acc->password_hash_b64 );

          fc::sha256 hash_obj = fc::sha256::hash( password + password_salt );
          if( hash_obj.data_size() != acc_password_hash.length() )
             return false;
          if( memcmp( hash_obj.data(), acc_password_hash.c_str(), hash_obj.data_size() ) != 0 )
             return false;
       }

       for (const std::string& api_name : acc->allowed_apis) {
          enable_api(api_name);
       }
       return true;
    }

    void login_api::enable_api( const std::string& api_name )
    {
       if (api_name == "database_api") {
          _database_api = std::make_shared< database_api >( std::ref( *_app.chain_database() ) );
       }
       else if (api_name == "network_broadcast_api") {
          _network_broadcast_api = std::make_shared< network_broadcast_api >( std::ref( _app ) );
       }
       else if (api_name == "history_api") {
          _history_api = std::make_shared< history_api >( _app );
       }
       else if (api_name == "secure_api") {
          _secure_api = std::make_shared< secure_api >( _app );
       }
       else if (api_name == "network_node_api") {
          _network_node_api = std::make_shared< network_node_api >( std::ref(_app) );
       }
       else if (api_name == "crypto_api") {
          _crypto_api = std::make_shared< crypto_api >();
       }
       else if (api_name == "debug_api")
       {
          // can only enable this API if the plugin was loaded
          if ( _app.get_plugin( "debug_witness" ))
             _debug_api = std::make_shared< graphene::debug_witness::debug_api >( std::ref(_app) );
       }
       return;
    }

    network_broadcast_api::network_broadcast_api(application& a):_app(a) {
       _applied_block_connection = _app.chain_database()->applied_block.connect([this](const signed_block& b){ on_applied_block(b); });
    }

    void network_broadcast_api::on_applied_block( const signed_block& b )
    {
       if( _callbacks.size() )
       {
          /// we need to ensure the database_api is not deleted for the life of the async operation
          auto capture_this = shared_from_this();
          for( uint32_t trx_num = 0; trx_num < b.transactions.size(); ++trx_num )
          {
             const auto& trx = b.transactions[trx_num];
             auto id = trx.id();
             auto itr = _callbacks.find(id);
             if( itr != _callbacks.end() )
             {
                auto block_num = b.block_num();
                auto& callback = _callbacks.find(id)->second;
                auto v = fc::variant( transaction_confirmation{ id, block_num, trx_num, trx }, GRAPHENE_MAX_NESTED_OBJECTS );
                fc::async( [capture_this,v,callback]() {
                   callback(v);
                } );
                //callback( fc::variant(transaction_confirmation{ id, block_num, trx_num, trx}) );
                 // TODO: 
//                fc::async( [capture_this,this,id,block_num,trx_num,trx,callback](){ callback( fc::variant(transaction_confirmation{ id, block_num, trx_num, trx}) ); } );
             }
          }
       }
    }

    void network_broadcast_api::broadcast_transaction(const signed_transaction& trx)
    {
       trx.validate();
       _app.chain_database()->push_transaction(trx);
       _app.p2p_node()->broadcast_transaction(trx);
    }

    void network_broadcast_api::broadcast_block( const signed_block& b )
    {
       _app.chain_database()->push_block(b);

       if (_app.p2p_node() != nullptr) {
          _app.p2p_node()->broadcast(net::block_message(b));
       }
    }

    signed_transaction network_broadcast_api::broadcast_bonus(string account_name, string amount)
    {
       FC_ASSERT(_app.chain_database());

       const auto& db = *_app.chain_database();
       auto dyn_props = db.get_dynamic_global_properties();

       const auto& accounts_by_name = db.get_index_type<account_index>().indices().get<by_name>();
       auto itr = accounts_by_name.find(account_name);
       FC_ASSERT(itr != accounts_by_name.end());

       const auto& assets_by_symbol = db.get_index_type<asset_index>().indices().get<by_symbol>();
       auto iter_asset = assets_by_symbol.find(EDC_ASSET_SYMBOL);
       FC_ASSERT(iter_asset != assets_by_symbol.end());

       const asset_object& asst = *iter_asset;
       const account_object& to_account = *itr;
       (void)asst;
       (void)to_account;

//       chain::asset_issue_operation op;
//       op.issuer = asst.issuer;
//       op.asset_to_issue = asst.amount_from_string(amount);
//       op.issue_to_account = to_account.id;
//
//       chain::signed_transaction trx;
//       trx.operations.push_back(op);
//       trx.set_expiration(db.get_slot_time(120));
//       trx.set_reference_block( dyn_props.head_block_id );
//
//       fc::optional<fc::ecc::private_key> priv_key = graphene::utilities::wif_to_key("some-wif-key");
//       FC_ASSERT( priv_key.valid(), "Malformed private key in _keys" );
//       trx.sign(*priv_key, db.get_chain_id());
//       broadcast_transaction(trx);
//       return trx;

////////////////////////////////////////////////////

//       auto fund = db.get_index_type<fund_index>().indices().get<by_name>().find("ecro10");
//       const chain::fund_object& obj = *fund;
//       std::cout << obj.fund_rates[1].day_percent << std::endl;

//       chain::fund_create_operation op;
//       op.name        = "ecro3";
//       op.description = "descr";
//       op.holder      = account_id_type();
//       op.fund_asset  = asset_id_type();
//       op.rates_reduction = 10;
//       op.period      = 20;
//       op.min_deposit = 30;
//       op.payment_rates[1] = 2;
//       op.fund_rates[1] = 2;
//       chain::transaction_evaluation_state eval(_app.chain_database().get());
//       _app.chain_database()->apply_operation(eval, op);
//
       return signed_transaction();

////////////////////////////////////////////////////

    }

    void network_broadcast_api::broadcast_transaction_with_callback(confirmation_callback cb, const signed_transaction& trx)
    {
       FC_THROW("Please, update your wallet");
    }

    void network_broadcast_api::broadcast_transaction_with_callback_new(confirmation_callback cb, const signed_transaction& trx)
    {
       trx.validate();
       _callbacks[trx.id()] = cb;
       _app.chain_database()->push_transaction(trx);

       if (_app.p2p_node() != nullptr) {
          _app.p2p_node()->broadcast_transaction(trx);
       }
    }

    network_node_api::network_node_api( application& a ) : _app( a )
    {
    }

    fc::variant_object network_node_api::get_info() const
    {
       fc::mutable_variant_object result = _app.p2p_node()->network_get_info();
       result["connection_count"] = _app.p2p_node()->get_connection_count();
       return result;
    }

    void network_node_api::add_node(const fc::ip::endpoint& ep)
    {
       _app.p2p_node()->add_node(ep);
    }

    std::vector<net::peer_status> network_node_api::get_connected_peers() const
    {
       return _app.p2p_node()->get_connected_peers();
    }

    std::vector<net::potential_peer_record> network_node_api::get_potential_peers() const
    {
       return _app.p2p_node()->get_potential_peers();
    }

    fc::variant_object network_node_api::get_advanced_node_parameters() const
    {
       return _app.p2p_node()->get_advanced_node_parameters();
    }

    void network_node_api::set_advanced_node_parameters(const fc::variant_object& params)
    {
       return _app.p2p_node()->set_advanced_node_parameters(params);
    }

    fc::api<network_broadcast_api> login_api::network_broadcast()const
    {
       FC_ASSERT(_network_broadcast_api);
       return *_network_broadcast_api;
    }

    fc::api<network_node_api> login_api::network_node()const
    {
       FC_ASSERT(_network_node_api);
       return *_network_node_api;
    }

    fc::api<database_api> login_api::database()const
    {
       FC_ASSERT(_database_api);
       return *_database_api;
    }

    fc::api<history_api> login_api::history() const
    {
       FC_ASSERT(_history_api);
       return *_history_api;
    }

    fc::api<secure_api> login_api::secure() const
    {
       FC_ASSERT(_secure_api);
       return *_secure_api;
    }

    fc::api<crypto_api> login_api::crypto() const
    {
       FC_ASSERT(_crypto_api);
       return *_crypto_api;
    }

    fc::api<graphene::debug_witness::debug_api> login_api::debug() const
    {
       FC_ASSERT(_debug_api);
       return *_debug_api;
    }

    vector<account_id_type> get_relevant_accounts( const object* obj )
    {
       vector<account_id_type> result;
       if( obj->id.space() == protocol_ids )
       {
          switch( (object_type)obj->id.type() )
          {
            case null_object_type:
            case base_object_type:
               return result;
            case account_object_type:{
               result.push_back( obj->id );
               break;
            } case asset_object_type:{
               const auto& aobj = dynamic_cast<const asset_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->issuer );
               break;
            } case force_settlement_object_type:{
               const auto& aobj = dynamic_cast<const force_settlement_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->owner );
               break;
            } case committee_member_object_type:{
               const auto& aobj = dynamic_cast<const committee_member_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->committee_member_account );
               break;
            } case witness_object_type:{
               const auto& aobj = dynamic_cast<const witness_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->witness_account );
               break;
            } case limit_order_object_type:{
               const auto& aobj = dynamic_cast<const limit_order_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->seller );
               break;
            } case call_order_object_type:{
               const auto& aobj = dynamic_cast<const call_order_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->borrower );
               break;
            } case custom_object_type:{
              break;
            } case proposal_object_type:{
               const auto& aobj = dynamic_cast<const proposal_object*>(obj);
               assert( aobj != nullptr );
               flat_set<account_id_type> impacted;
               flat_set<fund_id_type> impacted_fund_tmp;
               transaction_get_impacted_items( aobj->proposed_transaction, impacted, impacted_fund_tmp);
               result.reserve( impacted.size() );
               for( auto& item : impacted ) result.emplace_back(item);
               break;
            } case operation_history_object_type:{
               const auto& aobj = dynamic_cast<const operation_history_object*>(obj);
               assert( aobj != nullptr );
               flat_set<account_id_type> impacted;
               flat_set<fund_id_type> impacted_fund_tmp;
               operation_get_impacted_items( aobj->op, impacted, impacted_fund_tmp);
               result.reserve( impacted.size() );
               for( auto& item : impacted ) result.emplace_back(item);
               break;
            } case withdraw_permission_object_type:{
               const auto& aobj = dynamic_cast<const withdraw_permission_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->withdraw_from_account );
               result.push_back( aobj->authorized_account );
               break;
            } case vesting_balance_object_type:{
               const auto& aobj = dynamic_cast<const vesting_balance_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->owner );
               break;
            } case worker_object_type:{
               const auto& aobj = dynamic_cast<const worker_object*>(obj);
               assert( aobj != nullptr );
               result.push_back( aobj->worker_account );
               break;
            } case balance_object_type:{
               /** these are free from any accounts */
               break;
            } case restricted_account_object_type:
              case market_address_object_type:
              case fund_object_type:
              case cheque_object_type:
              case witnesses_info_object_type:
              break;
          }
       }
       else if( obj->id.space() == implementation_ids )
       {
          switch( (impl_object_type)obj->id.type() )
          {
                 case impl_global_property_object_type:
                  break;
                 case impl_dynamic_global_property_object_type:
                  break;
                 case impl_reserved0_object_type:
                  break;
                 case impl_asset_dynamic_data_object_type:
                  break;
                 case impl_asset_bitasset_data_object_type:
                  break;
                 case impl_account_balance_object_type:{
                  const auto& aobj = dynamic_cast<const account_balance_object*>(obj);
                  assert( aobj != nullptr );
                  result.push_back( aobj->owner );
                  break;
               } case impl_account_statistics_object_type:{
                  const auto& aobj = dynamic_cast<const account_statistics_object*>(obj);
                  assert( aobj != nullptr );
                  result.push_back( aobj->owner );
                  break;
               } case impl_transaction_history_object_type:{
                  const auto& aobj = dynamic_cast<const transaction_history_object*>(obj);
                  assert( aobj != nullptr );
                  flat_set<account_id_type> impacted;
                  flat_set<fund_id_type> impacted_fund_tmp;
                  transaction_get_impacted_items( aobj->trx, impacted, impacted_fund_tmp);
                  result.reserve( impacted.size() );
                  for( auto& item : impacted ) result.emplace_back(item);
                  break;
               } case impl_blinded_balance_object_type:{
                  const auto& aobj = dynamic_cast<const blinded_balance_object*>(obj);
                  assert( aobj != nullptr );
                  result.reserve( aobj->owner.account_auths.size() );
                  for( const auto& a : aobj->owner.account_auths )
                     result.push_back( a.first );
                  break;
               } case impl_block_summary_object_type:
                  break;
                 case impl_account_transaction_history_object_type:
                 case impl_chain_property_object_type:
                 case impl_witness_schedule_object_type:
                 case impl_budget_record_object_type:
                 case impl_special_authority_object_type:
                 case impl_buyback_object_type:
                 case impl_fba_accumulator_object_type:
                 case impl_account_mature_balance_object_type:
                 case impl_account_properties_object_type:
                 case impl_accounts_online_object_type:
                 case impl_bonus_balances_object_type:
                 case impl_fund_deposit_object_type:
                 case impl_fund_statistics_object_type:
                 case impl_fund_transaction_history_object_type:
                 case impl_fund_history_object_type:
                 case impl_settings_object_type:
                 case impl_blind_transfer2_object_type:
                  break;
          }
       }
       return result;
    } // end get_relevant_accounts( obj )

    vector<order_history_object> history_api::get_fill_order_history( asset_id_type a, asset_id_type b, uint32_t limit  )const
    {
       FC_ASSERT(_app.chain_database());
       const auto& db = *_app.chain_database();
       if( a > b ) std::swap(a,b);
       const auto& history_idx = db.get_index_type<graphene::market_history::history_index>().indices().get<by_key>();
       history_key hkey;
       hkey.base = a;
       hkey.quote = b;
       hkey.sequence = std::numeric_limits<int64_t>::min();

       uint32_t count = 0;
       auto itr = history_idx.lower_bound( hkey );
       vector<order_history_object> result;
       while( itr != history_idx.end() && count < limit)
       {
          if( itr->key.base != a || itr->key.quote != b ) break;
          result.push_back( *itr );
          ++itr;
          ++count;
       }

       return result;
    }

    inline void reserve_op(operation_history_object& h_obj)
    {
       if (h_obj.op.which() == operation::tag<blind_transfer2_operation>::value)
       {
          blind_transfer2_operation op;
          op.fee = asset(h_obj.result.get<asset>().amount, h_obj.result.get<asset>().asset_id);
          h_obj.op = op;
       }
       else if (h_obj.op.which() == operation::tag<cheque_create_operation>::value) {
          h_obj.op.get<cheque_create_operation>().code.clear();
       }
       else if (h_obj.op.which() == operation::tag<cheque_use_operation>::value) {
          h_obj.op.get<cheque_use_operation>().code.clear();
       }
    }

    vector<operation_history_object> history_api::get_accounts_history(unsigned limit) const
    {
       FC_ASSERT( _app.chain_database() );
       const auto& db = *_app.chain_database();
       FC_ASSERT( limit <= 100 );
       vector<operation_history_object> result;

       const auto& idx = db.get_index_type<operation_history_index>().indices().get<by_id>();
       for (auto rit = idx.rbegin(); rit != idx.crend(); ++rit)
       {
          if (result.size() < limit)
          {
             operation_history_object obj = *rit;
             db.clear_op(obj.op);
             result.emplace_back(std::move(obj));
          }
          if (result.size() == limit) { break; }
       }

       return result;
    }

    vector<operation_history_object> history_api::get_account_history( account_id_type account,
                                                                       operation_history_id_type stop, 
                                                                       unsigned limit, 
                                                                       operation_history_id_type start ) const
    {
       FC_ASSERT( _app.chain_database() );
       const auto& db = *_app.chain_database();       
       FC_ASSERT( limit <= 100 );
       vector<operation_history_object> result;

       const auto& stats = account(db).statistics(db);
       if (stats.most_recent_op == account_transaction_history_id_type()) { return result; }

       if (!db.find(stats.most_recent_op)) {
          return result;
       }
       const account_transaction_history_object* node = &stats.most_recent_op(db);

       if (start == operation_history_id_type()) {
          start = node->operation_id;
       }
       while (node && (node->operation_id.instance.value > stop.instance.value) && (result.size() < limit) )
       {
          if (node->operation_id.instance.value <= start.instance.value)
          {
             try
             {
                operation_history_object op_h = node->operation_id(db);
                reserve_op(op_h);
                result.push_back(std::move(op_h));
             } catch(fc::exception e) { break; }
          }
          if (node->next == account_transaction_history_id_type()) {
             node = nullptr;
          }
          else
          {
             auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
             if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
                node = nullptr;
             }
             else {
                node = &(*f_idx);
             }
          }
       }
       
       return result;
    }

   vector<listtransactions_result>
   history_api::listtransactions(account_id_type account, vector<string> addresses, int count) const
   {
      FC_ASSERT(_app.chain_database());
      const auto& db = *_app.chain_database();       
      FC_ASSERT(count <= 100);
      vector<listtransactions_result> result;
      const auto& stats = account(db).statistics(db);
      if (stats.most_recent_op == account_transaction_history_id_type()) { return result; }
      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const account_transaction_history_object* node = &stats.most_recent_op(db);
      const uint32_t current_block = db.head_block_num();
      while ((node != nullptr) && (result.size() < (uint32_t)count))
      {
         auto op_hist = node->operation_id(db);
         if (op_hist.op.which() == operation::tag<transfer_operation>::value)
         {
            auto ext = op_hist.op.get<transfer_operation>().extensions;
            auto tr_address = ext.begin() != ext.end() ? ext.begin()->get<string>(): "";
            if (addresses.size() && std::find(addresses.begin(), addresses.end(), tr_address) == addresses.end())
            {
               node = &node->next(db);
               continue;
            }
            try
            {
               result.push_back(listtransactions_result{op_hist.op.get<transfer_operation>(),
                                                        (int)(current_block - op_hist.block_num)});
            } catch (fc::exception e)
            { break; }
         }
         if (node->next == account_transaction_history_id_type()) {
            node = nullptr;
         }
         else
         {
            auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
            if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
               node = nullptr;
            }
            else {
               node = &(*f_idx);
            }
         }
      }
       
      return result;
   }

    vector<operation_history_object> history_api::get_relative_history( account_id_type account, 
                                                                                uint32_t stop, 
                                                                                unsigned limit, 
                                                                                uint32_t start) const
    {
       FC_ASSERT( _app.chain_database() );
       const auto& db = *_app.chain_database();
       FC_ASSERT(limit <= 100);
       vector<operation_history_object> result;
       if( start == 0 )
         start = account(db).statistics(db).total_ops;
       else start = min( account(db).statistics(db).total_ops, start );
       const auto& hist_idx = db.get_index_type<account_transaction_history_index>();
       const auto& by_seq_idx = hist_idx.indices().get<by_seq>();
       
       auto itr = by_seq_idx.upper_bound( boost::make_tuple( account, start ) );
       auto itr_stop = by_seq_idx.lower_bound( boost::make_tuple( account, stop ) );
       --itr;
       
       while ( itr != itr_stop && result.size() < limit )
       {
          try
          {
             operation_history_object op_h = itr->operation_id(db);
             reserve_op(op_h);
             result.push_back(std::move(op_h));
          } catch(fc::exception e) { break; }
          --itr;
       }
       
       return result;
    }

    vector<operation_history_object>
    history_api::get_account_operation_history(account_id_type account, unsigned operation_type, unsigned limit) const
    {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();       
      FC_ASSERT( limit <= 100 );

      vector<operation_history_object> result;
      const auto& stats = account(db).statistics(db);
      if( stats.most_recent_op == account_transaction_history_id_type() ) { return result; }

      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const account_transaction_history_object* node = &stats.most_recent_op(db);

      while (node && result.size() < limit)
      {
        try
        {
          const operation_history_object& hist = node->operation_id(db);
          if ((unsigned)hist.op.which() == operation_type)
          {
             operation_history_object op_h = hist;
             reserve_op(op_h);
             result.push_back(std::move(op_h));
          }
        }
        catch(fc::exception e) { break; }
        if ( node->next == account_transaction_history_id_type() ) {
           node = nullptr;
        }
        else
        {
           auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
           if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
              node = nullptr;
           }
           else {
              node = &(*f_idx);
           }
        }
      }
      
      return result;
    }

    vector<operation_history_object> history_api::get_account_operation_history2(
       account_id_type account
       , operation_history_id_type stop
       , unsigned limit
       , operation_history_id_type start
       , unsigned operation_type) const
    { 
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();       
      FC_ASSERT( limit <= 100 );
      vector<operation_history_object> result;
      const auto& stats = account(db).statistics(db);

      if (stats.most_recent_op == account_transaction_history_id_type()) { return result; }

      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const account_transaction_history_object* node = &stats.most_recent_op(db);

      if (start == operation_history_id_type()) {
         start = node->operation_id;
      }

      while (node && (node->operation_id.instance.value > stop.instance.value) && (result.size() < limit))
      {
        try
        {
          auto& hist = node->operation_id(db);
          if (node->operation_id.instance.value <= start.instance.value)
          {
             if ((unsigned)hist.op.which() == operation_type)
             {
                operation_history_object op_h = hist;
                reserve_op(op_h);
                result.push_back(std::move(op_h));
             }
          }
        }
        catch(fc::exception e) { break; }
        if (node->next == account_transaction_history_id_type()) {
           node = nullptr;
        }
        else
        {
           auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
           if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
              node = nullptr;
           }
           else {
              node = &(*f_idx);
           }
        }
      }

      return result;
   }

   vector<operation_history_object> history_api::get_account_operation_history3(
      account_id_type account_id
      , operation_history_id_type stop
      , unsigned limit
      , operation_history_id_type start
      , const vector<uint16_t>& operation_types) const
   {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();
      FC_ASSERT( limit <= 100 );
      vector<operation_history_object> result;
      const auto& stats = account_id(db).statistics(db);

      if (stats.most_recent_op == account_transaction_history_id_type()) { return result; }

      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const account_transaction_history_object* node = &stats.most_recent_op(db);

      if (start == operation_history_id_type()) {
         start = node->operation_id;
      }

      while (node && (node->operation_id.instance.value > stop.instance.value) && (result.size() < limit))
      {
         try
         {
            operation_history_object hist = node->operation_id(db);
            reserve_op(hist);

            if (node->operation_id.instance.value <= start.instance.value)
            {
               std::for_each(operation_types.begin(), operation_types.end(), [&account_id, &hist, &result](const uint16_t& op_type)
               {
                  if ((unsigned)hist.op.which() == op_type)
                  {
                     // fund_payment_operation
                     if (hist.op.which() == operation::tag<fund_payment_operation>::value)
                     {
                        if (hist.op.get<fund_payment_operation>().issue_to_account == account_id) {
                           result.push_back(std::move(hist));
                        }
                     }
                     else {
                        result.push_back(std::move(hist));
                     }
                  }
               });
            }
         }
         catch(fc::exception e) { break; }
         if (node->next == account_transaction_history_id_type()) {
            node = nullptr;
         }
         else
         {
            auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
            if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
               node = nullptr;
            }
            else {
               node = &(*f_idx);
            }
         }
      }

      return result;
   }

   vector<operation_history_object> history_api::get_account_operation_history4(
      account_id_type account
      , operation_history_id_type start
      , unsigned limit
      , const vector<uint16_t>& operation_types) const
   {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();
      FC_ASSERT( limit <= 100 );
      vector<operation_history_object> result;
      result.reserve(limit);

      const auto& idx = db.get_index_type<operation_history_index>().indices().get<by_id>();
      auto itr = idx.begin();

      auto is_valid_operation = [&operation_types, &account](const operation_history_object& op) -> bool
      {
         for (const uint16_t& op_type: operation_types)
         {
            if ((unsigned)op.op.which() == op_type)
            {
               // transfer operation
               if (op.op.which() == operation::tag<transfer_operation>::value)
               {
                  const transfer_operation& tr_op = op.op.get<transfer_operation>();
                  if ((tr_op.from == account) || (tr_op.to == account)) {
                     return true;
                  }
               }
               // ...
            }
         }
         return false;
      };

      if (start == operation_history_id_type())
      {
         while (itr != idx.end())
         {
            // find first valid operation
            if (is_valid_operation(*itr)) {
               break;
            }
            ++itr;
         }
      }
      else
      {
         itr = idx.find(start);
         if (itr == idx.end()) { return result; }
      }

      while (itr != idx.end())
      {
         if (result.size() == limit) { break;}

         const operation_history_object& op = *itr;

         if (is_valid_operation(op)) {
            result.emplace_back(op);
         }

         ++itr;
      }

      return result;
   }

   vector<operation_history_object> history_api::get_account_leasing_history(
      account_id_type account_id
      , const std::vector<fund_id_type> funds
      , unsigned limit
      , operation_history_id_type start) const
   {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();
      FC_ASSERT( limit <= 100 );
      vector<operation_history_object> result;
      const auto& stats = account_id(db).statistics(db);

      if (stats.most_recent_op == account_transaction_history_id_type()) { return result; }

      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const account_transaction_history_object* node = &stats.most_recent_op(db);

      if (start == operation_history_id_type()) {
         start = node->operation_id;
      }

      auto set_fund_validation = [&](const fund_id_type& fund_id, bool& status)
      {
         auto&& itr = std::find_if(funds.begin(), funds.end(), [&fund_id](const fund_id_type& item){
            return (item == fund_id);
         });
         if (itr != funds.end()) {
            status = true;
         }
      };

      while (node && (result.size() < limit))
      {
         try
         {
            operation_history_object hist = node->operation_id(db);
            reserve_op(hist);

            if (node->operation_id.instance.value <= start.instance.value)
            {
               const auto& op = hist.op.which();

               bool fund_is_valid = false;
               bool account_is_valid = false;

               if (op == operation::tag<fund_update_operation>::value)
               {
                  const fund_update_operation& inner_op = hist.op.get<fund_update_operation>();

                  set_fund_validation(inner_op.id, fund_is_valid);
                  if (fund_is_valid && (inner_op.from_account == account_id)) {
                     account_is_valid = true;
                  }
               }
               else if (op == operation::tag<fund_deposit_operation>::value)
               {
                  const fund_deposit_operation& inner_op = hist.op.get<fund_deposit_operation>();

                  set_fund_validation(inner_op.fund_id, fund_is_valid);
                  if (fund_is_valid && (inner_op.from_account == account_id)) {
                     account_is_valid = true;
                  }
               }
               else if (op == operation::tag<fund_withdrawal_operation>::value)
               {
                  const fund_withdrawal_operation& inner_op = hist.op.get<fund_withdrawal_operation>();

                  set_fund_validation(inner_op.fund_id, fund_is_valid);
                  if (fund_is_valid && (inner_op.issue_to_account == account_id)) {
                     account_is_valid = true;
                  }
               }
               else if (op == operation::tag<fund_payment_operation>::value)
               {
                  const fund_payment_operation& inner_op = hist.op.get<fund_payment_operation>();

                  set_fund_validation(inner_op.fund_id, fund_is_valid);
                  if (fund_is_valid && (inner_op.issue_to_account == account_id)) {
                     account_is_valid = true;
                  }
               }

               if (account_is_valid && fund_is_valid) {
                  result.push_back(std::move(hist));
               }
            }
         }
         catch(fc::exception e) { break; }

         if (node->next == account_transaction_history_id_type()) {
            node = nullptr;
         }
         else
         {
            auto f_idx = db.get_index_type<account_transaction_history_index>().indices().get<by_id>().find(node->next);
            if (f_idx == db.get_index_type<account_transaction_history_index>().indices().get<by_id>().end()) {
               node = nullptr;
            }
            else {
               node = &(*f_idx);
            }
         }
      }

      return result;
   }

   vector<operation_history_object> history_api::get_fund_history(fund_id_type fund_id
                                                                  , operation_history_id_type stop
                                                                  , unsigned limit
                                                                  , operation_history_id_type start
                                                                  , const vector<uint16_t>& operation_types) const
   { try {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();
      FC_ASSERT( limit <= 100 );
      vector<operation_history_object> result;
      const auto& stats = fund_id(db).statistics_id(db);

      if (stats.most_recent_op == fund_transaction_history_id_type()) { return result; }

      if (!db.find(stats.most_recent_op)) {
         return result;
      }
      const fund_transaction_history_object* node = &stats.most_recent_op(db);

      if (start == operation_history_id_type()) {
         start = node->operation_id;
      }

      while (node && (node->operation_id.instance.value > stop.instance.value) && (result.size() < limit))
      {
         try
         {
            auto& hist = node->operation_id(db);
            if (node->operation_id.instance.value <= start.instance.value)
            {
               std::for_each(operation_types.begin(), operation_types.end(), [&hist, &result](const uint16_t& op_type)
               {
                  if ((unsigned) hist.op.which() == op_type) {
                     result.push_back(hist);
                  }
               });
            }
         }
         catch(fc::exception e) { break; }
         if (node->next == fund_transaction_history_id_type()) {
            node = nullptr;
         }
         else
         {
            auto f_idx = db.get_index_type<fund_transaction_history_index>().indices().get<by_id>().find(node->next);
            if (f_idx == db.get_index_type<fund_transaction_history_index>().indices().get<by_id>().end()) {
               node = nullptr;
            }
            else {
               node = &(*f_idx);
            }
         }
      }

      return result;
   } FC_CAPTURE_AND_RETHROW( (fund_id)(stop)(limit)(start)(operation_types) ) }

   vector<fund_history_object::history_item>
   history_api::get_fund_payments_history(fund_id_type fund_id, uint32_t start, uint32_t limit) const
   { try {
      FC_ASSERT( _app.chain_database() );
      const auto& db = *_app.chain_database();
      FC_ASSERT( limit <= 100 );

      vector<fund_history_object::history_item> result;
      result.reserve(limit);

      const auto& hist = fund_id(db).history_id(db);

      std::vector<fund_history_object::history_item>::const_reverse_iterator rit = hist.items.rbegin();
      for (size_t i = 0; rit != hist.items.crend(); ++rit, ++i)
      {
         if ( (i >= start) && (result.size() < limit) ) {
            result.emplace_back(*rit);
         }
         if (result.size() == limit) { break; }
      }

      return result;
   } FC_CAPTURE_AND_RETHROW( (fund_id)(start)(limit) ) }

   flat_set<uint32_t> history_api::get_market_history_buckets()const
   {
      auto hist = _app.get_plugin<market_history_plugin>( "market_history" );
      FC_ASSERT( hist );
      return hist->tracked_buckets();
   }

   vector<bucket_object> history_api::get_market_history( asset_id_type a, asset_id_type b,
                                                          uint32_t bucket_seconds, fc::time_point_sec start, fc::time_point_sec end )const
   { try {
      FC_ASSERT(_app.chain_database());
      const auto& db = *_app.chain_database();
      vector<bucket_object> result;
      result.reserve(200);

      if (a > b) std::swap(a,b);

      const auto& bidx = db.get_index_type<bucket_index>();
      const auto& by_key_idx = bidx.indices().get<by_key>();

      auto itr = by_key_idx.lower_bound( bucket_key( a, b, bucket_seconds, start ) );
      while( itr != by_key_idx.end() && itr->key.open <= end && result.size() < 200 )
      {
         if (!(itr->key.base == a && itr->key.quote == b && itr->key.seconds == bucket_seconds)) {
            return result;
         }
         result.push_back(*itr);
         ++itr;
      }
      return result;

   } FC_CAPTURE_AND_RETHROW( (a)(b)(bucket_seconds)(start)(end) ) }

   fc::variants secure_api::get_objects(const vector<object_id_type>& ids) const
   {
      FC_ASSERT(_app.chain_database());
      const auto& db = *_app.chain_database();

      fc::variants result;
      result.reserve(ids.size());

      std::transform(ids.begin(), ids.end(), std::back_inserter(result),
                     [&](object_id_type id) -> fc::variant
                     {
                        if (auto obj = db.find_object(id)) {
                           return obj->to_variant();
                        }
                        return { };
                     });

      return result;
   }

   std::vector<cheque_object>
   secure_api::get_account_cheques(account_id_type account_id, uint32_t start, uint32_t limit) const
   {
      FC_ASSERT(_app.chain_database());
      const auto& db = *_app.chain_database();

      std::vector<cheque_object> result;

      const auto& idx = db.get_index_type<cheque_index>().indices().get<by_datetime_creation>();
      auto itr = idx.rbegin();
      uint32_t i = 0;

      while (itr != idx.rend())
      {
         if (itr->drawer == account_id)
         {
            if (i >= start) {
               result.emplace_back(*itr);
            }
            ++i;
         }

         for (const cheque_object::payee_item& item: itr->payees)
         {
            if (item.payee == account_id)
            {
               if (i >= start) {
                  result.emplace_back(*itr);
               }
               ++i;
            }
         }
         if (result.size() == limit) { break; }
         ++itr;
      }

      return result;
   }

   std::vector<blind_transfer2_object>
   secure_api::get_account_blind_transfers2(account_id_type account_id, uint32_t start, uint32_t limit) const
   {
      FC_ASSERT(_app.chain_database());
      const auto& db = *_app.chain_database();

      std::vector<blind_transfer2_object> result;

      const auto& idx = db.get_index_type<blind_transfer2_index>().indices().get<by_datetime>();
      auto itr = idx.rbegin();
      uint32_t i = 0;

      while (itr != idx.rend())
      {
         if ( (itr->from == account_id) || (itr->to == account_id) )
         {
            if (i >= start) {
               result.emplace_back(*itr);
            }
            ++i;
         }
         if (result.size() == limit) { break; }
         ++itr;
      }

      return result;
   }

   ///////////////////////////////////////////////////////////////////////////////

   crypto_api::crypto_api() { }

   /*blind_signature crypto_api::blind_sign( const extended_private_key_type& key, const blinded_hash& hash, int i ) {
      return fc::ecc::extended_private_key( key ).blind_sign( hash, i );
   }

   signature_type crypto_api::unblind_signature( const extended_private_key_type& key,
                                                  const extended_public_key_type& bob,
                                                  const blind_signature& sig,
                                                  const fc::sha256& hash,
                                                  int i ) {
      return fc::ecc::extended_private_key( key ).unblind_signature( extended_public_key( bob ), sig, hash, i );
   }*/

   commitment_type crypto_api::blind( const blind_factor_type& blind, uint64_t value ) {
      return fc::ecc::blind( blind, value );
   }

   blind_factor_type crypto_api::blind_sum( const std::vector<blind_factor_type>& blinds_in, uint32_t non_neg ) {
      return fc::ecc::blind_sum( blinds_in, non_neg );
   }

   bool crypto_api::verify_sum( const std::vector<commitment_type>& commits_in, const std::vector<commitment_type>& neg_commits_in, int64_t excess ) {
      return fc::ecc::verify_sum( commits_in, neg_commits_in, excess );
   }

   verify_range_result crypto_api::verify_range( const commitment_type& commit, const std::vector<char>& proof )
   {
      verify_range_result result;
      result.success = fc::ecc::verify_range( result.min_val, result.max_val, commit, proof );
      return result;
   }

   std::vector<char> crypto_api::range_proof_sign( uint64_t min_value,
                                                   const commitment_type& commit,
                                                   const blind_factor_type& commit_blind,
                                                   const blind_factor_type& nonce,
                                                   int8_t base10_exp,
                                                   uint8_t min_bits,
                                                   uint64_t actual_value ) {
    return fc::ecc::range_proof_sign( min_value, commit, commit_blind, nonce, base10_exp, min_bits, actual_value );
   }

   verify_range_proof_rewind_result crypto_api::verify_range_proof_rewind( const blind_factor_type& nonce,
                                                                           const commitment_type& commit,
                                                                           const std::vector<char>& proof )
   {
      verify_range_proof_rewind_result result;
      result.success = fc::ecc::verify_range_proof_rewind( result.blind_out,
                                                         result.value_out,
                                                         result.message_out,
                                                         nonce,
                                                         result.min_val,
                                                         result.max_val,
                                                         const_cast< commitment_type& >( commit ),
                                                         proof );
      return result;
   }

   range_proof_info crypto_api::range_get_info( const std::vector<char>& proof ) {
      return fc::ecc::range_get_info( proof );
   }

} } // graphene::app
