// see LICENSE.txt

#include <graphene/chain/database.hpp>

#include <graphene/chain/operation_history_object.hpp>
#include <graphene/protocol/fee_schedule.hpp>

#include <fc/io/fstream.hpp>

#include <fstream>
#include <functional>
#include <iostream>

namespace graphene { namespace chain {

database::database()
{
   initialize_indexes();
   initialize_evaluators();
}

database::~database()
{
   clear_pending();
}

void database::reindex(fc::path data_dir, const genesis_state_type& initial_allocation)
{ try {
   set_registrar_mode(true);
   enable_replay_process_status(true);

   if (!mt_times_file_created()) {
      mt_times_read();
   }

   ilog( "reindexing blockchain" );
   wipe(data_dir, false);
   open(data_dir, [&initial_allocation]{return initial_allocation;});

   auto start = fc::time_point::now();
   auto last_block = _block_id_to_block.last();
   if( !last_block ) {
      elog( "!no last block" );
      edump((last_block));
      return;
   }

   const auto last_block_num = last_block->block_num();

   ilog( "Replaying blocks..." );
   _undo_db.disable();

   for( uint32_t i = 1; i <= last_block_num; ++i )
   {
      if( i % 2000 == 0 ) std::cerr << "   " << double(i*100)/last_block_num << "%   "<<i << " of " <<last_block_num<<"   \n";
      fc::optional< signed_block > block = _block_id_to_block.fetch_by_number(i);
      if( !block.valid() )
      {
         wlog( "Reindexing terminated due to gap:  Block ${i} does not exist!", ("i", i) );
         uint32_t dropped_count = 0;
         while( true )
         {
            fc::optional< block_id_type > last_id = _block_id_to_block.last_id();
            // this can trigger if we attempt to e.g. read a file that has block #2 but no block #1
            if( !last_id.valid() )
               break;
            // we've caught up to the gap
            if( block_header::num_from_id( *last_id ) <= i )
               break;
            _block_id_to_block.remove( *last_id );
            dropped_count++;
         }
         wlog( "Dropped ${n} blocks from after the gap", ("n", dropped_count) );
         break;
      }
      apply_block(*block, skip_witness_signature |
                          skip_transaction_signatures |
                          skip_transaction_dupe_check |
                          skip_tapos_check |
                          skip_witness_schedule_check |
                          skip_authority_check);
   }
   _undo_db.enable();
   enable_replay_process_status(false);

   auto end = fc::time_point::now();
   ilog( "Done reindexing, elapsed time: ${t} sec", ("t",double((end-start).count())/1000000.0 ) );
} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::mt_times_create_file(const fc::path& data_dir)
{
   fc::path p = data_dir / "mt";

   std::ofstream ostrm(std::string(p.string()), std::ios::trunc);
   if (ostrm.is_open()) {
      ostrm << "0";
   }
}

void database::mt_times_add(uint32_t seconds)
{
   if (seconds == 0) { return; }

   fc::path p = get_data_dir() / "mt";

   if (fc::exists(p))
   {
      mt_times_read();

      std::ofstream ostrm(std::string(p.string()), std::ios::trunc);
      if (ostrm.is_open())
      {
         int64_t mt_seconds = _mt_seconds + seconds;
         ostrm << mt_seconds;
      }
   }
}

void database::mt_times_read()
{
   fc::path p = get_data_dir() / "mt";
   if (fc::exists(p))
   {
      std::ifstream istrm(std::string(p.string()), std::ofstream::in);
      if (istrm.is_open()) {
         istrm >> _mt_seconds;
      }
   }
}

void database::wipe(const fc::path& data_dir, bool include_blocks)
{
   ilog("Wiping database", ("include_blocks", include_blocks));

   if (_opened) {
      close();
   }

   object_database::wipe(data_dir);
   if (include_blocks) {
      fc::remove_all(data_dir / "database");
   }
}

void database::open(
   const fc::path& data_dir,
   std::function<genesis_state_type()> genesis_loader)
{
   try
   {
      object_database::open(data_dir);

      _block_id_to_block.open(data_dir / "database" / "block_num_to_block");

      mt_times_read();

      if (!find(global_property_id_type())) {
         init_genesis(genesis_loader());
      }

      fc::optional<signed_block> last_block = _block_id_to_block.last();
      if (last_block.valid())
      {
         _fork_db.start_block(*last_block);
         idump((last_block->id())(last_block->block_num()));
         if (last_block->id() != head_block_id()) {
              FC_ASSERT( head_block_num() == 0, "last block ID does not match current chain state" );
         }
      }
      _opened = true;
      //idump((head_block_id())(head_block_num()));
   }
   FC_CAPTURE_LOG_AND_RETHROW( (data_dir) )
}

void database::close(bool rewind)
{
   if (!_opened) { return; }

   // TODO:  Save pending tx's on close()
   clear_pending();

   // pop all of the blocks that we can given our undo history, this should
   // throw when there is no more undo history to pop
   if( rewind )
   {
      try
      {
         uint32_t cutoff = get_dynamic_global_properties().last_irreversible_block_num;

         //ilog( "Rewinding from ${head} to ${cutoff}", ("head",head_block_num())("cutoff",cutoff) );
         while( head_block_num() > cutoff )
         {
            block_id_type popped_block_id = head_block_id();
            pop_block();
            _fork_db.remove(popped_block_id); // doesn't throw on missing
            try {
               _block_id_to_block.remove(popped_block_id);
            }
            catch (const fc::key_not_found_exception&) {}
         }
      }
      catch (...) { }
   }

   // Since pop_block() will move tx's in the popped blocks into pending,
   // we have to clear_pending() after we're done popping to get a clean
   // DB state (issue #336).
   clear_pending();

   object_database::flush();
   object_database::close();

   if (_block_id_to_block.is_open()) {
      _block_id_to_block.close();
   }

   _fork_db.reset();

   _opened = false;
}

} }
