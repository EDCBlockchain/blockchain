// see LICENSE.txt

#pragma once

#include <graphene/protocol/buyback.hpp>

namespace graphene { namespace chain {

class database;

void evaluate_buyback_account_options( const database& db, const buyback_account_options& auth );

} } // graphene::chain
