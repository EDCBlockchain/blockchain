#include "wallet_api_impl.hpp"
#include <graphene/utilities/key_conversion.hpp>

/****
 * Methods to handle voting / workers / committee
 */

namespace graphene { namespace wallet { namespace detail {
   
   template<typename WorkerInit>
   static WorkerInit _create_worker_initializer( const variant& worker_settings )
   {
      WorkerInit result;
      from_variant( worker_settings, result, GRAPHENE_MAX_NESTED_OBJECTS );
      return result;
   }
   
   signed_transaction wallet_api_impl::create_committee_member(string owner_account, string url, bool broadcast /* = false */)
   { try {

      committee_member_create_operation committee_member_create_op;
      committee_member_create_op.committee_member_account = get_account_id(owner_account);
      committee_member_create_op.url = url;

      if (_remote_db->get_committee_member_by_account(committee_member_create_op.committee_member_account)) {
         FC_THROW("Account ${owner_account} is already a committee_member", ("owner_account", owner_account));
      }

      signed_transaction tx;
      tx.operations.push_back( committee_member_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }
   
   witness_object wallet_api_impl::get_witness(string owner_account)
   { try  {
      fc::optional<witness_id_type> witness_id = maybe_id<witness_id_type>(owner_account);
      if (witness_id)
      {
         std::vector<witness_id_type> ids_to_get;
         ids_to_get.push_back(*witness_id);
         std::vector<fc::optional<witness_object>> witness_objects = _remote_db->get_witnesses(ids_to_get);
         if (witness_objects.front())
            return *witness_objects.front();
         FC_THROW("No witness is registered for id ${id}", ("id", owner_account));
      }
      else
      {
         // then maybe it's the owner account
         try
         {
            account_id_type owner_account_id = get_account_id(owner_account);
            fc::optional<witness_object> witness = _remote_db->get_witness_by_account(owner_account_id);
            if (witness)
               return *witness;
            else
               FC_THROW("No witness is registered for account ${account}", ("account", owner_account));
         }
         catch (const fc::exception&)
         {
            FC_THROW("No account or witness named ${account}", ("account", owner_account));
         }
      }
   } FC_CAPTURE_AND_RETHROW( (owner_account) ) }
   
   committee_member_object wallet_api_impl::get_committee_member(string owner_account)
   {
      try
      {
         fc::optional<committee_member_id_type> committee_member_id = maybe_id<committee_member_id_type>(owner_account);
         if (committee_member_id)
         {
            std::vector<committee_member_id_type> ids_to_get;
            ids_to_get.push_back(*committee_member_id);
            std::vector<fc::optional<committee_member_object>> committee_member_objects = _remote_db->get_committee_members(ids_to_get);
            if (committee_member_objects.front())
               return *committee_member_objects.front();
            FC_THROW("No committee_member is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               account_id_type owner_account_id = get_account_id(owner_account);
               fc::optional<committee_member_object> committee_member = _remote_db->get_committee_member_by_account(owner_account_id);
               if (committee_member)
                  return *committee_member;
               else
                  FC_THROW("No committee_member is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or committee_member named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }
   
   signed_transaction wallet_api_impl::create_witness(string owner_account,
      string url,
      bool broadcast)
   { try {
      account_object witness_account = get_account(owner_account);
         fc::ecc::private_key active_private_key = get_private_key_for_account(witness_account);
      int witness_key_index = find_first_unused_derived_key_index(active_private_key);
      fc::ecc::private_key witness_private_key = derive_private_key(key_to_wif(active_private_key), witness_key_index);
      graphene::chain::public_key_type witness_public_key = witness_private_key.get_public_key();

      witness_create_operation witness_create_op;
      witness_create_op.witness_account = witness_account.id;
      witness_create_op.block_signing_key = witness_public_key;
      witness_create_op.url = url;

      if (_remote_db->get_witness_by_account(witness_create_op.witness_account))
         FC_THROW("Account ${owner_account} is already a witness", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( witness_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      _wallet.pending_witness_registrations[owner_account] = key_to_wif(witness_private_key);

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }
   
   signed_transaction wallet_api_impl::update_witness(string witness_name,
      string url,
      string block_signing_key,
      bool broadcast)
   { try {
      witness_object witness = get_witness(witness_name);
      account_object witness_account = get_account( witness.witness_account );
      fc::ecc::private_key active_private_key = get_private_key_for_account(witness_account);

      witness_update_operation witness_update_op;
      witness_update_op.witness = witness.id;
      witness_update_op.witness_account = witness_account.id;
      if( url != "" )
         witness_update_op.new_url = url;
      if( block_signing_key != "" )
         witness_update_op.new_signing_key = public_key_type( block_signing_key );

      signed_transaction tx;
      tx.operations.push_back( witness_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (witness_name)(url)(block_signing_key)(broadcast) ) }

   signed_transaction wallet_api_impl::create_worker(
      string owner_account,
      time_point_sec work_begin_date,
      time_point_sec work_end_date,
      share_type daily_pay,
      string name,
      string url,
      variant worker_settings,
      bool broadcast)
   {
      worker_initializer init;
      std::string wtype = worker_settings["type"].get_string();

      // TODO:  Use introspection to do this dispatch
      if( wtype == "burn" )
         init = _create_worker_initializer< burn_worker_initializer >( worker_settings );
      else if( wtype == "refund" )
         init = _create_worker_initializer< refund_worker_initializer >( worker_settings );
      else if( wtype == "vesting" )
         init = _create_worker_initializer< vesting_balance_worker_initializer >( worker_settings );
      else
      {
         FC_ASSERT( false, "unknown worker[\"type\"] value" );
      }

      worker_create_operation op;
      op.owner = get_account( owner_account ).id;
      op.work_begin_date = work_begin_date;
      op.work_end_date = work_end_date;
      op.daily_pay = daily_pay;
      op.name = name;
      op.url = url;
      op.initializer = init;

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction wallet_api_impl::update_worker_votes(
      string account,
      worker_vote_delta delta,
      bool broadcast)
   {
      account_object acct = get_account( account );

      // you could probably use a faster algorithm for this, but flat_set is fast enough :)
      flat_set< worker_id_type > merged;
      merged.reserve( delta.vote_for.size() + delta.vote_against.size() + delta.vote_abstain.size() );
      for( const worker_id_type& wid : delta.vote_for )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }
      for( const worker_id_type& wid : delta.vote_against )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }
      for( const worker_id_type& wid : delta.vote_abstain )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }

      // should be enforced by FC_ASSERT's above
      assert( merged.size() == delta.vote_for.size() + delta.vote_against.size() + delta.vote_abstain.size() );

      vector< object_id_type > query_ids;
      for( const worker_id_type& wid : merged )
         query_ids.push_back( wid );

      flat_set<vote_id_type> new_votes( acct.options.votes );

      fc::variants objects = _remote_db->get_objects( query_ids );
      for( const variant& obj : objects )
      {
         worker_object wo;
         from_variant( obj, wo, GRAPHENE_MAX_NESTED_OBJECTS );
         new_votes.erase( wo.vote_for );
         new_votes.erase( wo.vote_against );
         if( delta.vote_for.find( wo.id ) != delta.vote_for.end() )
            new_votes.insert( wo.vote_for );
         else if( delta.vote_against.find( wo.id ) != delta.vote_against.end() )
            new_votes.insert( wo.vote_against );
         else
            assert( delta.vote_abstain.find( wo.id ) != delta.vote_abstain.end() );
      }

      account_update_operation update_op;
      update_op.account = acct.id;
      update_op.new_options = acct.options;
      update_op.new_options->votes = new_votes;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }
   
   signed_transaction wallet_api_impl::vote_for_committee_member(string voting_account,
      string committee_member,
      bool approve,
      bool broadcast)
   { try {
      account_object voting_account_object = get_account(voting_account);
      account_id_type committee_member_owner_account_id = get_account_id(committee_member);
      fc::optional<committee_member_object> committee_member_obj = _remote_db->get_committee_member_by_account(committee_member_owner_account_id);
      if (!committee_member_obj)
         FC_THROW("Account ${committee_member} is not registered as a committee_member", ("committee_member", committee_member));
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(committee_member_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for committee_member ${committee_member}", ("account", voting_account)("committee_member", committee_member));
      }
      else
      {
         unsigned votes_removed = voting_account_object.options.votes.erase(committee_member_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for committee_member ${committee_member}", ("account", voting_account)("committee_member", committee_member));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(committee_member)(approve)(broadcast) ) }
   
   signed_transaction wallet_api_impl::vote_for_witness(string voting_account,
      string witness,
      bool approve,
      bool broadcast)
   { try {
      account_object voting_account_object = get_account(voting_account);
      account_id_type witness_owner_account_id = get_account_id(witness);
      fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(witness_owner_account_id);
      if (!witness_obj)
         FC_THROW("Account ${witness} is not registered as a witness", ("witness", witness));
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(witness_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for witness ${witness}", ("account", voting_account)("witness", witness));
      }
      else
      {
         unsigned votes_removed = voting_account_object.options.votes.erase(witness_obj->vote_id);
         if (!votes_removed)
            FC_THROW("Account ${account} is already not voting for witness ${witness}", ("account", voting_account)("witness", witness));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(witness)(approve)(broadcast) ) }
   
   signed_transaction wallet_api_impl::set_voting_proxy(string account_to_modify,
      fc::optional<string> voting_account,
      bool broadcast)
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);
      if (voting_account)
      {
         account_id_type new_voting_account_id = get_account_id(*voting_account);
         if (account_object_to_modify.options.voting_account == new_voting_account_id)
            FC_THROW("Voting proxy for ${account} is already set to ${voter}", ("account", account_to_modify)("voter", *voting_account));
         account_object_to_modify.options.voting_account = new_voting_account_id;
      }
      else
      {
         if (account_object_to_modify.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT)
            FC_THROW("Account ${account} is already voting for itself", ("account", account_to_modify));
         account_object_to_modify.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;
      }

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(voting_account)(broadcast) ) }
   
   signed_transaction wallet_api_impl::set_desired_witness_and_committee_member_count(string account_to_modify,
      uint16_t desired_number_of_witnesses,
      uint16_t desired_number_of_committee_members,
      bool broadcast)
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);

      if (account_object_to_modify.options.num_witness == desired_number_of_witnesses &&
          account_object_to_modify.options.num_committee == desired_number_of_committee_members)
         FC_THROW("Account ${account} is already voting for ${witnesses} witnesses and ${committee_members} committee_members",
                  ("account", account_to_modify)("witnesses", desired_number_of_witnesses)("committee_members",desired_number_of_committee_members));
      account_object_to_modify.options.num_witness = desired_number_of_witnesses;
      account_object_to_modify.options.num_committee = desired_number_of_committee_members;

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(desired_number_of_witnesses)(desired_number_of_committee_members)(broadcast) ) }

   std::string wallet_api_impl::dump_current_active_witnesses(bool only_nicknames)
   {
      auto global_props = get_global_properties();

      std::vector<witness_id_type> witness_ids;
      witness_ids.reserve(global_props.active_witnesses.size());

      for (const witness_id_type& item: global_props.active_witnesses) {
         witness_ids.emplace_back(item);
      }

      const std::vector<optional<witness_object>>& witnesses = _remote_db->get_witnesses(witness_ids);
      std::vector<account_id_type> account_ids;
      for (const auto& item: witnesses)
      {
         if (item) {
            account_ids.emplace_back(item->witness_account);
         }
      }

      const std::vector<optional<account_object>>& accounts = _remote_db->get_accounts(account_ids);

      std::ostringstream os;
      int count = 0;
      for (const auto& w_item: witnesses)
      {
         if (w_item)
         {
            const witness_object& witness = *w_item;

            auto itr = std::find_if(accounts.begin(), accounts.end(), [&](const optional<account_object>& acc)
            {
               if (acc && (acc->id == witness.witness_account)) { return true; }
               return false;
            });

            if (itr != accounts.end())
            {
               const account_object& acc = *(*itr);

               if (only_nicknames)
               {
                  os << acc.name;
                  if (&w_item != &witnesses.back()) {
                     os << '\n';
                  }
               }
               else
               {
                  os << std::string(witness.id) << ", (" << std::string(acc.id) << ", "
                     << acc.name << ", " << std::string(witness.vote_id) << ")" << '\n';
                  ++count;
               }
            }
         }
      }

      if (!only_nicknames) {
         os << "Count: " << count;
      }

      //std::cout << os.str() << std::endl;

      std::string fname = "active_witnesses.txt";
      std::ofstream ofile{fname};
      ofile << os.str();
      ofile.close();

      std::ifstream ifile{fname};
      if (!ifile.good()) {
         return ("Not saved: opening file " + fname + " - failed");
      }

      return ("success. Saved in " + fname);
   }

   std::vector<witnesses_with_missed_blocks_info> wallet_api_impl::get_witnesses_with_missed_blocks(bool only_nicknames, uint16_t limit) const
   {
      if (limit == 0) { FC_THROW("Limit must be greater than 0"); }

      auto global_props = get_global_properties();

      std::vector<witness_id_type> witness_ids;
      witness_ids.reserve(global_props.active_witnesses.size());

      for (const witness_id_type& item: global_props.active_witnesses) {
         witness_ids.emplace_back(item);
      }

      const std::vector<optional<witness_object>>& v = _remote_db->get_witnesses(witness_ids);
      std::vector<witness_object> witnesses;
      for (const auto& obj: v)
      {
         if (obj && obj->daily_missed > 0) {
            witnesses.push_back(*obj);
         }
      }

      std::sort(witnesses.begin(), witnesses.end(), [](const witness_object& obj1, const witness_object& obj2) {
         return (obj1.daily_missed > obj2.daily_missed);
      });

      std::vector<account_id_type> account_ids;
      for (const auto& item: witnesses) {
         account_ids.emplace_back(item.witness_account);
      }

      const std::vector<optional<account_object>>& accounts = _remote_db->get_accounts(account_ids);

      std::vector<witnesses_with_missed_blocks_info> result;
      std::ostringstream os;

      int i = 0;
      for (const witness_object& witness: witnesses)
      {
         if (i == limit) { break; }

         auto itr = std::find_if(accounts.begin(), accounts.end(), [&](const optional<account_object>& acc)
         {
            if (acc && (acc->id == witness.witness_account)) { return true; }
            return false;
         });

         if (itr != accounts.end())
         {
            const account_object& acc = *(*itr);

            if (only_nicknames) {
               os << acc.name << '\n';
            }
            else
            {
               witnesses_with_missed_blocks_info obj;
               obj.witness_id = witness.id;
               obj.account_id = acc.id;
               obj.login      = acc.name;
               obj.vote_id    = witness.vote_id;
               obj.daily_missed = witness.daily_missed;

               result.emplace_back(std::move(obj));
            }

            ++i;
         }
      }

      if (only_nicknames) {
         std::cout << os.str() << std::flush;
      }

      return result;
   }

   signed_transaction wallet_api_impl::propose_parameter_change(
      const string& proposing_account,
      fc::time_point_sec expiration_time,
      const variant_object& changed_values,
      bool broadcast)
   {
      //FC_ASSERT( !changed_values.contains("current_fees") );

      const chain_parameters& current_params = get_global_properties().parameters;
      chain_parameters new_params = current_params;
      fc::reflector<chain_parameters>::visit(
         fc::from_variant_visitor<chain_parameters>( changed_values, new_params , GRAPHENE_MAX_NESTED_OBJECTS)
         );

      committee_member_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }
   
    signed_transaction wallet_api_impl::propose_fee_change(
       const string& proposing_account,
       fc::time_point_sec expiration_time,
       const variant_object& changed_fees,
       bool broadcast)
   {
      const chain_parameters& current_params = get_global_properties().parameters;
      const fee_schedule_type& current_fees = *(current_params.current_fees);

      flat_map< int, fee_parameters > fee_map;
      fee_map.reserve( current_fees.parameters.size() );
      for( const fee_parameters& op_fee : current_fees.parameters )
         fee_map[ op_fee.which() ] = op_fee;
      uint32_t scale = current_fees.scale;

      for( const auto& item : changed_fees )
      {
         const string& key = item.key();
         if( key == "scale" )
         {
            int64_t _scale = item.value().as_int64();
            FC_ASSERT( _scale >= 0 );
            FC_ASSERT( _scale <= std::numeric_limits<uint32_t>::max() );
            scale = uint32_t( _scale );
            continue;
         }
         // is key a number?
         auto is_numeric = [&key]() -> bool
         {
            size_t n = key.size();
            for( size_t i=0; i<n; i++ )
            {
               if( !isdigit( key[i] ) )
                  return false;
            }
            return true;
         };

         int which;
         if( is_numeric() )
            which = std::stoi( key );
         else
         {
            const auto& n2w = _operation_which_map.name_to_which;
            auto it = n2w.find( key );
            FC_ASSERT( it != n2w.end(), "unknown operation" );
            which = it->second;
         }

         fee_parameters fp = from_which_variant< fee_parameters >( which, item.value(), GRAPHENE_MAX_NESTED_OBJECTS );
         fee_map[ which ] = fp;
      }

      fee_schedule_type new_fees;

      for( const std::pair< int, fee_parameters >& item : fee_map )
         new_fees.parameters.insert( item.second );
      new_fees.scale = scale;

      chain_parameters new_params = current_params;
      new_params.get_mutable_fees() = new_fees;

      committee_member_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.committee_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.current_fees->set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

}}} // namespace graphene::wallet::detail