// see LICENSE.txt

#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/types.hpp>

namespace graphene { namespace chain {

class transfer_to_blind_evaluator : public evaluator<transfer_to_blind_evaluator>
{
   public:
      typedef transfer_to_blind_operation operation_type;

      void_result do_evaluate( const transfer_to_blind_operation& o );
      void_result do_apply( const transfer_to_blind_operation& o ) ;

      virtual void pay_fee() override;
};

class transfer_from_blind_evaluator : public evaluator<transfer_from_blind_evaluator>
{
   public:
      typedef transfer_from_blind_operation operation_type;

      void_result do_evaluate( const transfer_from_blind_operation& o );
      void_result do_apply( const transfer_from_blind_operation& o ) ;

      virtual void pay_fee() override;
};

class blind_transfer_evaluator : public evaluator<blind_transfer_evaluator>
{
   public:
      typedef blind_transfer_operation operation_type;

      void_result do_evaluate( const blind_transfer_operation& o );
      void_result do_apply( const blind_transfer_operation& o ) ;

      virtual void pay_fee() override;
};

} } // namespace graphene::chain
