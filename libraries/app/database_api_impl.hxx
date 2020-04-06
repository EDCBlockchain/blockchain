
#include <graphene/app/database_api.hpp>

#include <fc/bloom_filter.hpp>

#define GET_REQUIRED_FEES_MAX_RECURSION 4

namespace graphene { namespace app {

class database_api_impl : public std::enable_shared_from_this<database_api_impl>
{
   public:
      explicit database_api_impl( graphene::chain::database& db );
      ~database_api_impl();

      // Objects
      fc::variants get_objects(const vector<object_id_type>& ids)const;
      optional<object_id_type> get_last_object_id(object_id_type id) const;

      // Subscriptions
      void set_subscribe_callback( std::function<void(const variant&)> cb, bool clear_filter );
      void set_pending_transaction_callback( std::function<void(const variant&)> cb );
      void set_block_applied_callback( std::function<void(const variant& block_id)> cb );
      void cancel_all_subscriptions();

      // Blocks and transactions
      optional<block_header> get_block_header(uint32_t block_num)const;
      optional<signed_block> get_block(uint32_t block_num);
      optional<signed_block> get_block_by_id(string block_num);
      processed_transaction get_transaction(uint32_t block_num, uint32_t trx_in_block);
      optional<signed_block> get_block_reserved(uint32_t block_num);

      // Globals
      chain_property_object get_chain_properties() const;
      global_property_object get_global_properties() const;
      fc::variant_object get_config() const;
      chain_id_type get_chain_id() const;
      dynamic_global_property_object get_dynamic_global_properties() const;

      // Keys
      vector<vector<account_id_type>> get_key_references( vector<public_key_type> key ) const;

      // Addresses
      vector<vector<account_id_type>> get_address_references( vector<address> key ) const;

      // market addresses
      fc::optional<account_id_type> get_market_reference(const address& key) const;

      // Accounts
      vector<optional<account_object>> get_accounts(const vector<account_id_type>& account_ids) const;

      std::pair<unsigned, vector<address>>
      get_account_addresses(const string& name_or_id, unsigned from, unsigned limit) const;

      std::map<string,full_account> get_full_accounts( const vector<string>& names_or_ids, bool subscribe );
      optional<bonus_balances_object> get_bonus_balances ( string name_or_id ) const;
      optional<account_object> get_account_by_name( string name ) const;
      optional<account_object> get_account_by_name_or_id(const string& name_or_id) const;
      optional<account_object> get_account_by_vote_id(const vote_id_type& v_id) const;
      Unit get_referrals( optional<account_object> account ) const;
      ref_info get_referrals_by_id( optional<account_object> account ) const;
      vector<SimpleUnit> get_accounts_info(vector<optional<account_object>> accounts);
      fc::variant_object get_user_count_by_ranks() const;
      int64_t get_user_count_with_balances(fc::time_point_sec start, fc::time_point_sec end) const;
      std::pair<uint32_t, std::vector<account_id_type>> get_users_with_asset(const asset_id_type& asst, uint32_t start, uint32_t limit) const;
      vector<account_id_type> get_account_references(account_id_type account_id) const;
      optional<restricted_account_object> get_restricted_account(account_id_type account_id) const;
      vector<optional<account_object>> lookup_account_names(const vector<string>& account_names)const;
      map<string,account_id_type> lookup_accounts(const string& lower_bound_name, uint32_t limit)const;
      uint64_t get_account_count()const;

      // Balances
      vector<asset> get_account_balances(account_id_type id, const flat_set<asset_id_type>& assets)const;
      vector<asset> get_named_account_balances(const std::string& name, const flat_set<asset_id_type>& assets)const;
      vector<balance_object> get_balance_objects( const vector<address>& addrs )const;
      vector<asset> get_vested_balances( const vector<balance_id_type>& objs )const;
      vector<vesting_balance_object> get_vesting_balances( account_id_type account_id )const;

      // Assets
      vector<optional<asset_object>> get_assets(const vector<asset_id_type>& asset_ids)const;
      vector<asset_object>           list_assets(const string& lower_bound_symbol, uint32_t limit)const;
      vector<optional<asset_object>> lookup_asset_symbols(const vector<string>& symbols_or_ids)const;

      // Funds
      vector<fund_object>             list_funds() const;
      const fund_object*              get_fund_by_name_or_id(const std::string& fund_name_or_id) const;
      const fund_object               get_fund(const std::string& fund_name_or_id) const;
      fund_object                     get_fund_by_owner(const std::string& account_name_or_id) const;
      vector<fund_deposit_object>     get_fund_deposits(const std::string& fund_name_or_id, uint32_t start, uint32_t limit) const;
      pair<vector<fund_deposit_object>, uint32_t>
                                      get_all_fund_deposits_by_period(uint32_t period, uint32_t start, uint32_t limit) const;
      asset                           get_fund_deposits_amount_by_account(fund_id_type fund_id, account_id_type account_id) const;
      vector<fund_deposit_object>     get_account_deposits(account_id_type account_id, uint32_t start, uint32_t limit) const;
      vector<market_address_object>   get_market_addresses(account_id_type account_id, uint32_t start, uint32_t limit) const;

      // Markets / feeds
      vector<limit_order_object>      get_limit_orders(asset_id_type a, asset_id_type b, uint32_t limit)const;
      vector<call_order_object>       get_call_orders(asset_id_type a, uint32_t limit)const;
      vector<force_settlement_object> get_settle_orders(asset_id_type a, uint32_t limit)const;
      map<account_id_type, uint16_t>  get_online_info()const;
      vector<call_order_object>       get_margin_positions( const account_id_type& id )const;
      void subscribe_to_market(std::function<void(const variant&)> callback, asset_id_type a, asset_id_type b);
      void unsubscribe_from_market(asset_id_type a, asset_id_type b);
      market_ticker                   get_ticker( const string& base, const string& quote )const;
      market_volume                   get_24_volume( const string& base, const string& quote )const;
      order_book                      get_order_book( const string& base, const string& quote, unsigned limit = 50 )const;
      vector<market_trade>            get_trade_history( const string& base, const string& quote, fc::time_point_sec start, fc::time_point_sec stop, unsigned limit = 100 )const;

      // Witnesses
      vector<optional<witness_object>> get_witnesses(const vector<witness_id_type>& witness_ids)const;
      fc::optional<witness_object> get_witness_by_account(account_id_type account)const;
      map<string, witness_id_type> lookup_witness_accounts(const string& lower_bound_name, uint32_t limit)const;
      uint64_t get_witness_count()const;

      // Committee members
      vector<optional<committee_member_object>> get_committee_members(const vector<committee_member_id_type>& committee_member_ids)const;
      fc::optional<committee_member_object> get_committee_member_by_account(account_id_type account)const;
      map<string, committee_member_id_type> lookup_committee_member_accounts(const string& lower_bound_name, uint32_t limit)const;

      // Votes
      vector<variant> lookup_vote_ids( const vector<vote_id_type>& votes )const;
      vector<account_id_type> get_voting_accounts(const vote_id_type& vote) const;

      // Authority / validation
      std::string get_transaction_hex(const signed_transaction& trx)const;
      set<public_key_type> get_required_signatures( const signed_transaction& trx, const flat_set<public_key_type>& available_keys )const;
      set<public_key_type> get_potential_signatures( const signed_transaction& trx )const;
      set<address> get_potential_address_signatures( const signed_transaction& trx )const;
      fee_schedule get_current_fee_schedule() const;

      bool verify_authority( const signed_transaction& trx )const;
      bool verify_account_authority( const string& name_or_id, const flat_set<public_key_type>& signers )const;
      processed_transaction validate_transaction( const signed_transaction& trx )const;
      vector< fc::variant > get_required_fees( const vector<operation>& ops, asset_id_type id )const;

      // Proposed transactions
      vector<proposal_object> get_proposed_transactions( account_id_type id )const;

      // Blinded balances
      vector<blinded_balance_object> get_blinded_balances( const flat_set<commitment_type>& commitments )const;

      optional<cheque_info_object> get_cheque_by_code(const std::string& code) const;

      std::pair<optional<asset_object>, optional<asset_object>>
      get_asset_objects(const asset& amount, bool is_blind = false) const;

      transfer_fee_info get_required_transfer_fee(const asset& amount) const;
      transfer_fee_info get_required_blind_transfer_fee(const asset& amount) const;
      transfer_fee_info get_required_cheque_fee(const asset& amount, uint32_t count) const;
      max_transfer_info get_max_transfer_amount_and_fee(const asset& amount, bool is_blind) const;
      max_transfer_info get_max_cheque_amount_and_fee(const asset& amount) const;
      asset get_burnt_asset(asset_id_type id) const;

      void clear_ops(std::vector<operation>& ops) {
         _db.clear_ops(ops);
      }

      //private:
      template<typename T>
      void subscribe_to_item( const T& i )const
      {
         auto vec = fc::raw::pack(i);
         if( !_subscribe_callback )
            return;

         if( !is_subscribed_to_item(i) )
         {
            idump((i));
            _subscribe_filter.insert( vec.data(), vec.size() );//(vecconst char*)&i, sizeof(i) );
         }
      }

      template<typename T>
      bool is_subscribed_to_item( const T& i )const
      {
         if( !_subscribe_callback )
            return false;
         return true;
         return _subscribe_filter.contains( i );
      }

      void broadcast_updates( const vector<variant>& updates );

      /** called every time a block is applied to report the objects that were changed */
      void on_objects_changed(const vector<object_id_type>& ids);
      void on_objects_removed(const vector<const object*>& objs);
      void on_applied_block();

      mutable fc::bloom_filter                               _subscribe_filter;
      std::function<void(const fc::variant&)> _subscribe_callback;
      std::function<void(const fc::variant&)> _pending_trx_callback;
      std::function<void(const fc::variant&)> _block_applied_callback;

      boost::signals2::scoped_connection _change_connection;
      boost::signals2::scoped_connection _removed_connection;
      boost::signals2::scoped_connection _applied_block_connection;
      boost::signals2::scoped_connection _pending_trx_connection;
      map<pair<asset_id_type,asset_id_type>, std::function<void(const variant&)>> _market_subscriptions;
      graphene::chain::database& _db;
};

} } // graphene::app
