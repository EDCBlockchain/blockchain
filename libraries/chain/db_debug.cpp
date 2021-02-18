// see LICENSE.txt

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>

namespace graphene { namespace chain {

/**
 *  This method dumps the state of the blockchain in a semi-human readable form for the
 *  purpose of tracking down funds and mismatches in currency allocation
 */
void database::debug_dump()
{
   const auto& db = *this;
   const asset_dynamic_data_object& core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);

   const auto& acc_balance_index = db.get_index_type<account_balance_index>().indices();
   const auto& bal_index = db.get_index_type<balance_index>().indices();
   const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();

   map<asset_id_type,share_type> total_balances;
   map<asset_id_type,share_type> total_debts;
   share_type core_in_orders;
   share_type reported_core_in_orders;

   for( const account_balance_object& a : acc_balance_index )
   {
    //  idump(("balance")(a));
      total_balances[a.asset_type] += a.balance;
   }
   for( const account_statistics_object& s : statistics_index )
   {
    //  idump(("statistics")(s));
      reported_core_in_orders += s.total_core_in_orders;
   }
   for( const limit_order_object& o : db.get_index_type<limit_order_index>().indices() )
   {
 //     idump(("limit_order")(o));
      auto for_sale = o.amount_for_sale();
      if( for_sale.asset_id == asset_id_type() ) core_in_orders += for_sale.amount;
      total_balances[for_sale.asset_id] += for_sale.amount;
   }
   for (const call_order_object& o : db.get_index_type<call_order_index>().indices())
   {
//      idump(("call_order")(o));
      auto col = o.get_collateral();
      if (col.asset_id == asset_id_type()) core_in_orders += col.amount;
      if (col.amount > -1) {
         total_balances[col.asset_id] += col.amount;
      }
      total_debts[o.get_debt().asset_id] += o.get_debt().amount;
   }
   for( const asset_object& asset_obj : db.get_index_type<asset_index>().indices() )
   {
      total_balances[asset_obj.id] += asset_obj.dynamic_asset_data_id(db).accumulated_fees;
      total_balances[asset_id_type()] += asset_obj.dynamic_asset_data_id(db).fee_pool;
//      edump((total_balances[asset_obj.id])(asset_obj.dynamic_asset_data_id(db).current_supply ) );
   }

   // db_init.cpp:database::init_genesis() applies initial balances, we must take it into account too
   for (const balance_object& item: bal_index)
   {
      if (item.asset_type() == asset_id_type()) {
         total_balances[asset_id_type()] += (item.balance.amount - 1);
      }
   }

   if (total_balances[asset_id_type()].value != core_asset_data.current_supply.value) {
      edump((total_balances[asset_id_type()].value)(core_asset_data.current_supply.value));
   }

   /*
   const auto& vbidx = db.get_index_type<simple_index<vesting_balance_object>>();
   for( const auto& s : vbidx )
   {
//      idump(("vesting_balance")(s));
   }
   */
}

void debug_apply_update( database& db, const fc::variant_object& vo )
{
   static const uint8_t
      db_action_nil = 0,
      db_action_create = 1,
      db_action_write = 2,
      db_action_update = 3,
      db_action_delete = 4;

   // "_action" : "create"   object must not exist, unspecified fields take defaults
   // "_action" : "write"    object may exist, is replaced entirely, unspecified fields take defaults
   // "_action" : "update"   object must exist, unspecified fields don't change
   // "_action" : "delete"   object must exist, will be deleted

   // if _action is unspecified:
   // - delete if object contains only ID field
   // - otherwise, write

   object_id_type oid;
   uint8_t action = db_action_nil;
   auto it_id = vo.find("id");
   FC_ASSERT( it_id != vo.end() );

   from_variant( it_id->value(), oid );
   action = ( vo.size() == 1 ) ? db_action_delete : db_action_write;

   from_variant( vo["id"], oid );
   if( vo.size() == 1 )
      action = db_action_delete;
   auto it_action = vo.find("_action" );
   if( it_action != vo.end() )
   {
      const std::string& str_action = it_action->value().get_string();
      if( str_action == "create" )
         action = db_action_create;
      else if( str_action == "write" )
         action = db_action_write;
      else if( str_action == "update" )
         action = db_action_update;
      else if( str_action == "delete" )
         action = db_action_delete;
   }

   auto& idx = db.get_index( oid );

   switch( action )
   {
      case db_action_create:
         FC_ASSERT( false );
         break;
      case db_action_write:
         db.modify( db.get_object( oid ), [&]( object& obj )
         {
            idx.object_default( obj );
            idx.object_from_variant( vo, obj, GRAPHENE_MAX_NESTED_OBJECTS );
         } );
         break;
      case db_action_update:
         db.modify( db.get_object( oid ), [&]( object& obj )
         {
            idx.object_from_variant( vo, obj, GRAPHENE_MAX_NESTED_OBJECTS );
         } );
         break;
      case db_action_delete:
         db.remove( db.get_object( oid ) );
         break;
      default:
         FC_ASSERT( false );
   }
}

void database::apply_debug_updates()
{
   block_id_type head_id = head_block_id();
   auto it = _node_property_object.debug_updates.find( head_id );
   if( it == _node_property_object.debug_updates.end() )
      return;
   for( const fc::variant_object& update : it->second )
      debug_apply_update( *this, update );
}

void database::debug_update( const fc::variant_object& update )
{
   block_id_type head_id = head_block_id();
   auto it = _node_property_object.debug_updates.find( head_id );
   if( it == _node_property_object.debug_updates.end() )
      it = _node_property_object.debug_updates.emplace( head_id, std::vector< fc::variant_object >() ).first;
   it->second.emplace_back( update );

   fc::optional<signed_block> head_block = fetch_block_by_id( head_id );
   FC_ASSERT( head_block.valid() );

   // What the last block does has been changed by adding to node_property_object, so we have to re-apply it
   pop_block();
   push_block( *head_block );
}

} }
