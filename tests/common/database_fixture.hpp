// see LICENSE.txt

#pragma once

#include <boost/filesystem/path.hpp>

#include <fc/io/json.hpp>

#include <graphene/app/application.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/utilities/tempdir.hpp>

#include <iostream>

using namespace graphene::db;

extern uint32_t GRAPHENE_TESTING_GENESIS_TIMESTAMP;

#define PUSH_TX \
   graphene::chain::test::_push_transaction

#define PUSH_BLOCK \
   graphene::chain::test::_push_block

// See below
#define REQUIRE_OP_VALIDATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   op.validate(); \
   op.field = temp; \
}
#define REQUIRE_OP_EVALUATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = temp; \
   db.push_transaction( trx, ~0 ); \
}

#define GRAPHENE_REQUIRE_THROW( expr, exc_type )          \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_REQUIRE_THROW begin "        \
         << req_throw_info << std::endl;                  \
   BOOST_REQUIRE_THROW( expr, exc_type );                 \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_REQUIRE_THROW end "          \
         << req_throw_info << std::endl;                  \
}

#define GRAPHENE_CHECK_THROW( expr, exc_type )            \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_CHECK_THROW begin "          \
         << req_throw_info << std::endl;                  \
   BOOST_CHECK_THROW( expr, exc_type );                   \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "GRAPHENE_CHECK_THROW end "            \
         << req_throw_info << std::endl;                  \
}

#define REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, exc_type ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   GRAPHENE_REQUIRE_THROW( op.validate(), exc_type ); \
   op.field = temp; \
}
#define REQUIRE_OP_VALIDATION_FAILURE( op, field, value ) \
   REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, fc::exception )

#define REQUIRE_THROW_WITH_VALUE_2(op, field, value, exc_type) \
{  \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   GRAPHENE_REQUIRE_THROW(db.push_transaction(trx, ~0), exc_type); \
}

#define REQUIRE_THROW_WITH_VALUE( op, field, value ) \
   REQUIRE_THROW_WITH_VALUE_2( op, field, value, fc::exception )

///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); trx.clear()

#define PREP_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
   public_key_type name ## _public_key = name ## _private_key.get_public_key();

#define ACTOR(name) \
   PREP_ACTOR(name) \
   const account_object& name = create_account(BOOST_PP_STRINGIZE(name), name ## _public_key); \
   account_id_type name ## _id = name.id; (void)name ## _id;

#define SET_ACTOR_CAN_CREATE_ASSET(account_id) \
   transaction_evaluation_state account_id ## eval_state(&db); \
   allow_create_asset_operation account_id ## op; \
   account_id ## op.to_account = account_id; \
   account_id ## op.value = true; \
   trx.operations.push_back(account_id ## op); \
   db.apply_operation(account_id ## eval_state, account_id ## op);

#define GET_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name)); \
   const account_object& name = get_account_by_name(BOOST_PP_STRINGIZE(name)); \
   account_id_type name ## _id = name.id; \
   (void)name ##_id

#define ACTORS_IMPL(r, data, elem) ACTOR(elem)
#define ACTORS(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, ~, names)

#define INITIAL_WITNESS_COUNT (11u)
#define INITIAL_COMMITTEE_MEMBER_COUNT INITIAL_WITNESS_COUNT

#define ISSUE_BY_NAME(z, n, data) issue_uia(BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM(2, 0, data), n), BOOST_PP_TUPLE_ELEM(2, 1, data));
#define ISSUES_BY_NAMES(name_prefix, suf_from, suf_to, asset) \
BOOST_PP_REPEAT_FROM_TO(suf_from, suf_to, ISSUE_BY_NAME, (name_prefix, asset))

#define MAKE_DEPOSIT_PAYMENT_BY_NAME(z, n, data) \
{ \
   fund_deposit_operation op; \
   op.amount = BOOST_PP_TUPLE_ELEM(2, 1, data); \
   op.fee = asset(); \
   op.from_account = BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM(2, 0, data), n), _id); \
   op.period = 50; \
   op.fund_id = fund.id; \
   set_expiration(db, trx); \
   trx.operations.push_back(std::move(op)); \
   PUSH_TX(db, trx, ~0); \
   trx.clear(); \
}
#define MAKE_DEPOSIT_PAYMENTS_BY_NAMES(name_prefix, suf_from, suf_to, amount) \
BOOST_PP_REPEAT_FROM_TO(suf_from, suf_to, MAKE_DEPOSIT_PAYMENT_BY_NAME, (name_prefix, amount))

#define CHANGE_REFERRER_MULTIPLE_IMPL(r, data, elem) get_account_by_name(elem).get_id() BOOST_PP_COMMA_IF(1)
#define CHANGE_REFERRER_MULTIPLE(names, referrer) \
{ \
   update_accounts_referrer_operation op; \
   op.fee = asset(); \
   op.accounts = { \
      BOOST_PP_SEQ_FOR_EACH(CHANGE_REFERRER_MULTIPLE_IMPL, , names) \
   }; \
   op.new_referrer = get_account_by_name(referrer).get_id(); \
   set_expiration(db, trx); \
   trx.operations.push_back(std::move(op)); \
   PUSH_TX(db, trx, ~0); \
   trx.clear(); \
}

namespace fc {

   /**
    * Set value of a named variable in the program options map if the variable has not been set.
    */
   template<typename T>
   static void set_option( boost::program_options::variables_map& options, const std::string& name, const T& value ) {
      options.emplace( name, boost::program_options::variable_value( value, false ) );
   }

} // namespace fc

namespace graphene { namespace chain {

namespace test {
   /// set a reasonable expiration time for the transaction
   void set_expiration( const database& db, transaction& tx );

   bool _push_block( database& db, const signed_block& b, uint32_t skip_flags = 0 );
      processed_transaction _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags = 0 );

} // ns test

struct database_fixture_base {
   // the reason we use an app is to exercise the indexes of built-in plugins
   graphene::app::application app;
   genesis_state_type genesis_state;
   chain::database &db;
   signed_transaction trx;
   public_key_type committee_key;
   account_id_type committee_account;
   fc::ecc::private_key private_key;
   fc::ecc::private_key init_account_priv_key;
   public_key_type init_account_pub_key;

   fc::temp_directory data_dir;
   bool skip_key_index_test = false;
   uint32_t anon_acct_count;

   const std::string current_test_name;
   const std::string current_suite_name;

   database_fixture_base();
   virtual ~database_fixture_base();

   static void init_genesis( database_fixture_base& fixture );
   static std::shared_ptr<boost::program_options::variables_map> init_options( database_fixture_base& fixture );

   static fc::ecc::private_key generate_private_key(string seed);
   string generate_anon_acct_name();
   static void verify_asset_supplies( const database& db );
   void verify_history_plugin_index( )const;
   signed_block generate_block(uint32_t skip = ~0,
                               const fc::ecc::private_key& key = generate_private_key("null_key"),
                               int miss_blocks = 0);

   /**
    * @brief Generates block_count blocks
    * @param block_count number of blocks to generate
    */
   void generate_blocks(uint32_t block_count);

   /**
    * @brief Generates blocks until the head block time matches or exceeds timestamp
    * @param timestamp target time to generate blocks until
    */
   void generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true, uint32_t skip = ~0);

   account_create_operation make_account(
      const std::string& name = "nathan",
      public_key_type = public_key_type()
      );

   account_create_operation make_account(
      const std::string& name,
      const account_object& registrar,
      const account_object& referrer,
      uint8_t referrer_percent = 100,
      public_key_type key = public_key_type()
      );

   void force_global_settle(const asset_object& what, const price& p);
   operation_result force_settle(account_id_type who, asset what)
   { return force_settle(who(db), what); }
   operation_result force_settle(const account_object& who, asset what);
   void update_feed_producers(asset_id_type mia, flat_set<account_id_type> producers)
   { update_feed_producers(mia(db), producers); }
   void update_feed_producers(const asset_object& mia, flat_set<account_id_type> producers);
   void publish_feed(asset_id_type mia, account_id_type by, const price_feed& f)
   { publish_feed(mia(db), by(db), f); }
   void publish_feed(const asset_object& mia, const account_object& by, const price_feed& f);
   const call_order_object* borrow(account_id_type who, asset what, asset collateral)
   { return borrow(who(db), what, collateral); }
   const call_order_object* borrow(const account_object& who, asset what, asset collateral);
   void cover(account_id_type who, asset what, asset collateral_freed)
   { cover(who(db), what, collateral_freed); }
   void cover(const account_object& who, asset what, asset collateral_freed);

   const asset_object& get_asset( const string& symbol )const;
   const account_object& get_account_by_name( const string& name )const;
   const account_object& get_account_by_id( const account_id_type& id )const;
   const asset_object& create_bitasset(const string& name,
                                       account_id_type issuer = GRAPHENE_WITNESS_ACCOUNT,
                                       uint16_t market_fee_percent = 100 /*1%*/,
                                       uint16_t flags = charge_market_fee);
   const asset_object& create_prediction_market(const string& name,
                                       account_id_type issuer = GRAPHENE_WITNESS_ACCOUNT,
                                       uint16_t market_fee_percent = 100 /*1%*/,
                                       uint16_t flags = charge_market_fee);
   const asset_object& create_user_issued_asset( const string& name, 
                                                 asset_parameters ext_options = asset_parameters(),
                                                 account_id_type issuer = account_id_type(6) );
   const asset_object& create_user_issued_asset( const string& name,
                                                 const account_object& issuer,
                                                 uint16_t flags,
                                                 asset_parameters ext_options = asset_parameters() );
   void issue_uia( const account_object& recipient, asset amount );
   void issue_uia( account_id_type recipient_id, asset amount );

   const account_object& create_account(
      const string& name,
      const public_key_type& key = public_key_type()
      );

   const account_object& create_account(
      const string& name,
      const account_object& registrar,
      const account_object& referrer,
      uint8_t referrer_percent = 100,
      const public_key_type& key = public_key_type()
      );

   const account_object& create_account(
      const string& name,
      const private_key_type& key,
      const account_id_type& registrar_id = account_id_type(),
      const account_id_type& referrer_id = account_id_type(),
      uint8_t referrer_percent = 100
      );

   const committee_member_object& create_committee_member( const account_object& owner );
   const witness_object& create_witness(account_id_type owner,
                                        const fc::ecc::private_key& signing_private_key = generate_private_key("null_key"));
   const witness_object& create_witness(const account_object& owner,
                                        const fc::ecc::private_key& signing_private_key = generate_private_key("null_key"));
   uint64_t fund( const account_object& account, const asset& amount = asset(500000) );
   digest_type digest( const transaction& tx );
   void sign( signed_transaction& trx, const fc::ecc::private_key& key );
   const limit_order_object* create_sell_order( account_id_type user, const asset& amount, const asset& recv );
   const limit_order_object* create_sell_order( const account_object& user, const asset& amount, const asset& recv );
   asset cancel_limit_order( const limit_order_object& order );
   void transfer( account_id_type from, account_id_type to, const asset& amount, const asset& fee = asset() );
   void transfer( const account_object& from, const account_object& to, const asset& amount, const asset& fee = asset() );
   void fund_fee_pool( const account_object& from, const asset_object& asset_to_fund, const share_type amount );
   void enable_fees();
   void change_fees( const fee_parameters::flat_set_type& new_params, uint32_t new_scale = 0 );
   void upgrade_to_lifetime_member( account_id_type account );
   void upgrade_to_lifetime_member( const account_object& account );
   void upgrade_to_annual_member( account_id_type account );
   void upgrade_to_annual_member( const account_object& account );
   void print_market( const string& syma, const string& symb )const;
   string pretty( const asset& a )const;
   void print_limit_order( const limit_order_object& cur )const;
   void print_call_orders( )const;
   void print_joint_market( const string& syma, const string& symb )const;
   int64_t get_balance( account_id_type account, asset_id_type a )const;
   int64_t get_balance_for_bonus( account_id_type account, asset_id_type a )const;
   int64_t get_balance( const account_object& account, const asset_object& a )const;
   vector< operation_history_object > get_operation_history( account_id_type account_id )const;

   void create_edc(share_type max_supply = 10000000000, asset base = asset(2, CORE_ASSET), asset quote = asset(1, EDC_ASSET));
   void create_test_asset();

   /** FUNDS **/

   void make_fund(
    const string& fund_name
    , const fund_options& options
    , account_id_type owner = ALPHA_ACCOUNT_ID);

   void make_cheque(
      const string& rcp_code
      , fc::time_point_sec expiration_datetime
      , asset_id_type asset_id
      , share_type amount_per_payee
      , uint32_t payees_count
      , account_id_type owner
      , asset fee = asset());

   void use_cheque(
   const string& rcp_code,
   account_id_type to_account);

}; // struct database_fixture_base

template<typename F>
struct database_fixture_init : database_fixture_base
{
   database_fixture_init()
   {
      F::init( *this );

//      asset_id_type mpa1_id(1);
//      BOOST_REQUIRE( mpa1_id(db).is_market_issued() );
//      BOOST_CHECK( mpa1_id(db).bitasset_data(db).asset_id == mpa1_id );
   }

   static void init( database_fixture_init<F>& fixture )
   { try {
      fixture.data_dir = fc::temp_directory( graphene::utilities::temp_directory_path() );
      fc::create_directories( fixture.data_dir.path() );
      F::init_genesis( fixture );
      fc::json::save_to_file( fixture.genesis_state, fixture.data_dir.path() / "genesis.json" );
      auto options = F::init_options( fixture );
      fc::set_option( *options, "genesis-json", boost::filesystem::path(fixture.data_dir.path() / "genesis.json") );
      fixture.app.initialize( fixture.data_dir.path(), options );
      fixture.app.startup();

      fixture.generate_block();

      test::set_expiration( fixture.db, fixture.trx );
   } FC_LOG_AND_RETHROW() }
};

struct database_fixture : database_fixture_init<database_fixture>
{
};

} }
