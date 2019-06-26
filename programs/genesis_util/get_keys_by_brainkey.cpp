//
// Created by dim4egster2 on 6/19/19.
//
#include <iostream>
#include <string>

#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/utilities/key_conversion.hpp>

using namespace std;

fc::ecc::private_key derive_private_key( const std::string& prefix_string,
                                         int sequence_number )
{
   std::string sequence_string = std::to_string(sequence_number);
   fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
   fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
   return derived_key;
}

string normalize_brain_key( string s )
{
   size_t i = 0, n = s.length();
   std::string result;
   char c;
   result.reserve( n );

   bool preceded_by_whitespace = false;
   bool non_empty = false;
   while( i < n )
   {
      c = s[i++];
      switch( c )
      {
         case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
            preceded_by_whitespace = true;
            continue;

         case 'a': c = 'A'; break;
         case 'b': c = 'B'; break;
         case 'c': c = 'C'; break;
         case 'd': c = 'D'; break;
         case 'e': c = 'E'; break;
         case 'f': c = 'F'; break;
         case 'g': c = 'G'; break;
         case 'h': c = 'H'; break;
         case 'i': c = 'I'; break;
         case 'j': c = 'J'; break;
         case 'k': c = 'K'; break;
         case 'l': c = 'L'; break;
         case 'm': c = 'M'; break;
         case 'n': c = 'N'; break;
         case 'o': c = 'O'; break;
         case 'p': c = 'P'; break;
         case 'q': c = 'Q'; break;
         case 'r': c = 'R'; break;
         case 's': c = 'S'; break;
         case 't': c = 'T'; break;
         case 'u': c = 'U'; break;
         case 'v': c = 'V'; break;
         case 'w': c = 'W'; break;
         case 'x': c = 'X'; break;
         case 'y': c = 'Y'; break;
         case 'z': c = 'Z'; break;

         default:
            break;
      }
      if (preceded_by_whitespace && non_empty) {
         result.push_back(' ');
      }

      result.push_back(c);
      preceded_by_whitespace = false;
      non_empty = true;
   }
   return result;
}

int main( int argc, char** argv )
{

   std::string dev_key_prefix;
   bool need_help = false;

   if( argc < 2 )
      need_help = true;
   else
   {
      dev_key_prefix = argv[1];

      if ((dev_key_prefix == "-h") || (dev_key_prefix == "--help"))
         need_help = true;
   }

   if( need_help )
   {
      std::cerr << "\n";
      std::cerr << "get_key_by_brainkey <\"brainkey\"> ...\n"
                   "\n"
                   "example:\n"
                   "\n"
                   "get_key_by_brainkey \"i am a brain key\"\n"
                   "\n";
      return 1;
   }


   std::string brain_key = argv[1];
   string normalized_brain_key = normalize_brain_key( brain_key );
   fc::ecc::private_key owner_privkey = derive_private_key( normalized_brain_key, 0 );
   //result.wif_priv_key = key_to_wif( priv_key );
   //result.pub_key = priv_key.get_public_key();

   bool comma = false;

   auto show_key = [&]( const fc::ecc::private_key& priv_key )
   {
      fc::limited_mutable_variant_object mvo(5);
      graphene::chain::public_key_type pub_key = priv_key.get_public_key();
      mvo( "private_key", graphene::utilities::key_to_wif( priv_key ) )
      ( "public_key", std::string( pub_key ) )
      ( "address", graphene::chain::address( pub_key ) )
      ;
      if( comma )
         std::cout << ",\n";
      std::cout << fc::json::to_string( fc::mutable_variant_object(mvo) );
      comma = true;
   };

   std::cout<<"brain key:"<<brain_key<<std::endl;
   std::cout << "[";
   show_key(owner_privkey);
   std::cout << "]\n";
}