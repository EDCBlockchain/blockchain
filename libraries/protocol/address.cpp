// see LICENSE.txt

#include <graphene/protocol/address.hpp>
#include <graphene/protocol/pts_address.hpp>
#include <fc/crypto/base58.hpp>
#include <algorithm>
#include <fc/io/raw.hpp>
#include <iostream>

namespace graphene {  namespace protocol {

   address::address(){}

   address::address( const std::string& base58str )
   {
      if (base58str.length() == 0) { return; }

      std::string prefix( GRAPHENE_ADDRESS_PREFIX );

      FC_ASSERT(is_valid(base58str, prefix), "${str}", ("str", base58str));

      std::vector<char> v = fc::from_base58( base58str.substr( prefix.size() ) );
      memcpy( (char*)addr._hash, v.data(), std::min<size_t>( v.size()-4, sizeof( addr ) ) );
   }

   bool address::is_valid( const std::string& base58str, const std::string& prefix )
   {
      const size_t prefix_len = prefix.size();
      if( base58str.size() <= prefix_len )
          return false;
      if( base58str.substr( 0, prefix_len ) != prefix )
          return false;
      std::vector<char> v;
      try {
		       v = fc::from_base58( base58str.substr( prefix_len ) );
	     }

      catch( const fc::parse_error_exception& e ) {
		       return false;
	     }

      if( v.size() != sizeof( fc::ripemd160 ) + 4 )
          return false;
      const fc::ripemd160 checksum = fc::ripemd160::hash( v.data(), v.size() - 4 );
      if( memcmp( v.data() + 20, (char*)checksum._hash, 4 ) != 0 )
          return false;
      return true;
   }

   address::address( const fc::ecc::public_key& pub )
   {
       auto dat = pub.serialize();
       addr = fc::ripemd160::hash( fc::sha512::hash( (char*) dat.data(), dat.size() ) );
   }

   address::address( uint32_t blocknum, uint32_t tr_num, uint32_t num, bool with_fix)
   {
      std::string mask = "000000000000000000000000000000000";
      int blocknum_length = 12;
      int tr_length = 3;
      int counter_length = 3;
      std::string res = std::string(mask, 0, blocknum_length - std::to_string(blocknum).size()) + std::to_string(blocknum);

      if (with_fix)
      {
         // transaction number
         res += std::string(std::string(tr_length, '0'), 0, tr_length - std::to_string(tr_num).size()) + std::to_string(tr_num);
         // counter number
         res += std::string(std::string(counter_length, '0'), 0, counter_length - std::to_string(num).size()) + std::to_string(num);
      }
      else
      {
         res += "000" + std::to_string(tr_num);
         if (num > 0) {
            res += "000" + std::to_string(num);
         }
      }

      res += std::string(mask, res.size());
      addr = fc::ripemd160::hash( fc::sha512::hash( res.c_str(), sizeof( res ) ) );
   }

   address::address( const pts_address& ptsaddr )
   {
       addr = fc::ripemd160::hash( (char*)&ptsaddr, sizeof( ptsaddr ) );
   }

   address::address( const fc::ecc::public_key_data& pub )
   {
      addr = fc::ripemd160::hash( fc::sha512::hash( (char*) pub.data(), pub.size() ) );
   }

   address::address( const graphene::protocol::public_key_type& pub )
   {
      addr = fc::ripemd160::hash( fc::sha512::hash( (char*) pub.key_data.data(), pub.key_data.size() ) );
   }

   address::operator std::string()const
   {
      char bin_addr[24];
      static_assert( sizeof(bin_addr) >= sizeof(addr) + 4, "address size mismatch" );
      memcpy( bin_addr, addr.data(), sizeof(addr) );
      auto checksum = fc::ripemd160::hash( addr.data(), sizeof(addr) );
      memcpy( bin_addr + sizeof(addr), (char*)&checksum._hash[0], 4 );
      return GRAPHENE_ADDRESS_PREFIX + fc::to_base58( bin_addr, sizeof(bin_addr) );
   }

} } // namespace graphene::protocol

namespace fc
{
   void to_variant( const graphene::protocol::address& var,  variant& vo, uint32_t max_depth )
    {
        vo = std::string(var);
    }
   void from_variant( const variant& var,  graphene::protocol::address& vo, uint32_t max_depth )
    {
        vo = graphene::protocol::address( var.as_string() );
    }
}

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::address )
