#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>
#include <graphene/chain/database.hpp>

#include <iostream>

namespace graphene { namespace chain {

      /**
       * @class receipt_object
       * @ingroup object
       * @ingroup protocol
       *
       * This object contains information and logic of receipt
       */
      class receipt_object: public db::abstract_object<receipt_object>
      {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = receipt_object_type;

         receipt_id_type           get_id() const { return id; }
         const fc::time_point_sec& get_creation_datetime() const { return datetime_creation; }
         const fc::time_point_sec& get_expiration_datetime() const { return datetime_expiration; }
         const fc::time_point_sec& get_used_datetime() const { return datetime_used; }
         const asset&              get_amount() const { return amount; }
         const account_id_type&    get_maker() const { return maker; }
         const account_id_type&    get_taker() const { return taker; }
         const receipt_statuses&   get_receipt_status() const { return receipt_status; }

         // receipt activation code
         std::string receipt_code;

         // receipt object: date and time
         fc::time_point_sec datetime_creation;

         // receipt expiration date and time
         fc::time_point_sec datetime_expiration;

         // receipt date and time, when it was used
         fc::time_point_sec datetime_used;

         // account which has created receipt
         account_id_type maker;

         // account which has used receipt (is actual for 'receipt_type::receipt_use')
         account_id_type taker;

         // amount of receipt
         asset amount;

         // receipt type (send / receive)
         receipt_statuses receipt_status = receipt_new;

      }; // receipt_object

      struct by_maker;
      struct by_taker;
      struct by_code;
      struct by_exp_datetime;
      struct by_creation_datetime;
      /**
       * @ingroup object_index
       */
      typedef multi_index_container<
      receipt_object,
      indexed_by<
            ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
            ordered_unique<tag<by_code>, member<receipt_object, std::string, &receipt_object::receipt_code>>,
            ordered_non_unique<tag<by_maker>, member<receipt_object, account_id_type, &receipt_object::maker>>,
            ordered_non_unique<tag<by_taker>, member<receipt_object, account_id_type, &receipt_object::taker>>,
            ordered_non_unique<tag<by_exp_datetime>, member<receipt_object, fc::time_point_sec, &receipt_object::datetime_expiration>>,
      ordered_non_unique<tag<by_creation_datetime>, member<receipt_object, fc::time_point_sec, &receipt_object::datetime_creation>>
         >
      > receipt_object_index_type;

      /**
       * @ingroup object_index
       */
      typedef generic_index<receipt_object, receipt_object_index_type> receipt_index;

/////////////////////////////////////////////////////////////////////////////////////////////////

}}

FC_REFLECT_DERIVED( graphene::chain::receipt_object,
                    (graphene::db::object),
                    (receipt_code)
                    (datetime_creation)
                    (datetime_expiration)
                    (datetime_used)
                    (maker)
                    (taker)
                    (amount)
                    (receipt_status)
                  );