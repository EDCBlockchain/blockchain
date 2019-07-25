#include <graphene/chain/cheque_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>
#include <boost/range.hpp>

#include <iostream>
#include <iomanip>

namespace graphene { namespace chain {

   void cheque_object::allocate_payees(uint32_t payees_count) {
      payees.resize(payees_count);
   }

   void cheque_object::process_payee(account_id_type payee, database& db)
   {
      // if cheque is already used then exit...
      if (status == cheque_status::cheque_used) { return; }

      for (uint32_t i = 0; i < payees.size(); i++)
      {
         // if have unused payee item
         if (payees[i].status == cheque_status::cheque_new)
         {
            // adjust balance for payee user
            db.adjust_balance(payee, asset(amount_payee, asset_id));

            // fill used payee item
            payees[i].status = cheque_status::cheque_used;
            payees[i].payee = payee;
            payees[i].datetime_used = db.head_block_time();

            amount_remaining -= amount_payee;

            // if last item was used then cheque status is 'cheque_used'
            if (i == (payees.size() - 1))
            {
               status = cheque_status::cheque_used;
               datetime_used = db.head_block_time();
            }
            break;
         }
      }
   }

} } // graphene::chain