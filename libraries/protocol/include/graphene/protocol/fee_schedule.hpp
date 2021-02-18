// see LICENSE.txt

#pragma once
#include <graphene/protocol/operations.hpp>

namespace graphene { namespace protocol {

   template<typename T> struct transform_to_fee_parameters;
   template<typename ...T>
   struct transform_to_fee_parameters<fc::static_variant<T...>>
   {
      using type = fc::static_variant< typename T::fee_parameters_type... >;
   };
   using fee_parameters = transform_to_fee_parameters<operation>::type;

   /**
    *  @brief contains all of the parameters necessary to calculate the fee for any operation
    */
   struct fee_schedule
   {
      fee_schedule();

      static fee_schedule get_default();

      /**
       *  Finds the appropriate fee parameter struct for the operation
       *  and then calculates the appropriate fee.
       */
      asset calculate_fee( const operation& op, const price& core_exchange_rate = price::unit_price() )const;
      asset set_fee( operation& op, const price& core_exchange_rate = price::unit_price() )const;

      void zero_all_fees();

      /**
       *  Validates all of the parameters are present and accounted for.
       */
      void validate()const;

      template<typename Operation>
      const typename Operation::fee_parameters_type& get()const
      {
         auto itr = parameters.find( typename Operation::fee_parameters_type() );
         FC_ASSERT( itr != parameters.end() );
         return itr->template get<typename Operation::fee_parameters_type>();
      }
      template<typename Operation>
      typename Operation::fee_parameters_type& get()
      {
         auto itr = parameters.find( typename Operation::fee_parameters_type() );
         FC_ASSERT( itr != parameters.end() );
         return itr->template get<typename Operation::fee_parameters_type>();
      }

      /**
       *  @note must be sorted by fee_parameters.which() and have no duplicates
       */
      fee_parameters::flat_set_type parameters;
      uint32_t                 scale = GRAPHENE_100_PERCENT; ///< fee * scale / GRAPHENE_100_PERCENT
   };

   typedef fee_schedule fee_schedule_type;

} } // graphene::protocol

FC_REFLECT_TYPENAME( graphene::protocol::fee_parameters )
FC_REFLECT( graphene::protocol::fee_schedule, (parameters)(scale) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::fee_schedule )