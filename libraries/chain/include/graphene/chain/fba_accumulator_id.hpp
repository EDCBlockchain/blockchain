// see LICENSE.txt

#pragma once

#include <graphene/protocol/types.hpp>

namespace graphene { namespace chain {

/**
 * An object will be created at genesis for each of these FBA accumulators.
 */
enum graphene_fba_accumulator_id_enum
{
   fba_accumulator_id_transfer_to_blind = 0,
   fba_accumulator_id_blind_transfer,
   fba_accumulator_id_transfer_from_blind,
   fba_accumulator_id_count
};

} }
