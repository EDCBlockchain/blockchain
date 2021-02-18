// see LICENSE.txt

#pragma once

#include <graphene/protocol/types.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>

namespace graphene { namespace protocol {
   struct pts_address;

   /**
    *  @brief a 160 bit hash of a public key
    *
    *  An address can be converted to or from a base58 string with 32 bit checksum.
    *
    *  An address is calculated as ripemd160( sha512( compressed_ecc_public_key ) )
    *
    *  When converted to a string, checksum calculated as the first 4 bytes ripemd160( address ) is
    *  appended to the binary address before converting to base58.
    */
   class address
   {
      public:
       address(); ///< constructs empty / null address
       address( uint32_t blocknum, uint32_t tr_num, uint32_t num = 0, bool with_fix = false);
       explicit address( const std::string& base58str );   ///< converts to binary, validates checksum
       address( const fc::ecc::public_key& pub ); ///< converts to binary
       explicit address( const fc::ecc::public_key_data& pub ); ///< converts to binary
       address( const pts_address& pub ); ///< converts to binary
       address( const public_key_type& pubkey );

       static bool is_valid( const std::string& base58str, const std::string& prefix = GRAPHENE_ADDRESS_PREFIX );

       explicit operator std::string()const; ///< converts to base58 + checksum

//       friend size_t hash_value( const address& v ) {
//          const void* tmp = static_cast<const void*>(v.addr._hash+2);
//
//          const size_t* tmp2 = reinterpret_cast<const size_t*>(tmp);
//          return *tmp2;
//       }
       fc::ripemd160 addr;
   };
   inline bool operator == ( const address& a, const address& b ) { return a.addr == b.addr; }
   inline bool operator != ( const address& a, const address& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const address& a, const address& b ) { return a.addr <  b.addr; }

} } // namespace graphene::protocol

namespace fc
{
   void to_variant( const graphene::protocol::address& var,  fc::variant& vo, uint32_t max_depth = 1 );
   void from_variant( const fc::variant& var,  graphene::protocol::address& vo, uint32_t max_depth = 1 );
}

FC_REFLECT( graphene::protocol::address, (addr) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::address )
