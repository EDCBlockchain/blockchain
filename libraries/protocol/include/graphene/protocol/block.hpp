// see LICENSE.txt

#pragma once
#include <graphene/protocol/transaction.hpp>

namespace graphene { namespace protocol {

   struct block_header
   {
      digest_type                   digest()const;
      block_id_type                 previous;
      uint32_t                      block_num()const { return num_from_id(previous) + 1; }
      fc::time_point_sec            timestamp;
      witness_id_type               witness;
      checksum_type                 transaction_merkle_root;
      extensions_type               extensions;

      static uint32_t num_from_id(const block_id_type& id);
   };

   struct signed_block_header : public block_header
   {
      block_id_type              id()const;
      fc::ecc::public_key        signee()const;
      void                       sign( const fc::ecc::private_key& signer );
      bool                       validate_signee( const fc::ecc::public_key& expected_signee )const;

      signature_type             witness_signature;
   };

   struct signed_block : public signed_block_header
   {

      block_id_type block_id;
      uint32_t block_number;
      checksum_type calculate_merkle_root()const;
      vector<processed_transaction> transactions;
      void update() {
          block_id = id();
          block_number = block_num();
      }
   };

} } // graphene::protocol

FC_REFLECT( graphene::protocol::block_header, (previous)(timestamp)(witness)(transaction_merkle_root)(extensions) )
FC_REFLECT_DERIVED( graphene::protocol::signed_block_header, (graphene::protocol::block_header), (witness_signature) )
FC_REFLECT_DERIVED( graphene::protocol::signed_block, (graphene::protocol::signed_block_header), (transactions)(block_id)(block_number) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::block_header)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::signed_block_header)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::signed_block)