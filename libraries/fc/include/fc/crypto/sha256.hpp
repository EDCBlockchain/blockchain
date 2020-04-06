#pragma once
#include <boost/endian/buffers.hpp>
#include <fc/fwd.hpp>
#include <fc/string.hpp>
#include <fc/io/raw_fwd.hpp>

namespace fc
{

class sha256 
{
  public:
    sha256();
    explicit sha256( const string& hex_str );
    explicit sha256( const char *data, size_t size );

    string str()const;
    operator string()const;

    char*    data()const;
    static constexpr size_t data_size() { return 256 / 8; }

    static sha256 hash( const char* d, uint32_t dlen );
    static sha256 hash( const string& );
    static sha256 hash( const sha256& );

    template<typename T>
    static sha256 hash( const T& t ) 
    { 
      sha256::encoder e; 
      fc::raw::pack(e,t);
      return e.result(); 
    } 

    class encoder 
    {
      public:
        encoder();
        ~encoder();

        void write( const char* d, uint32_t dlen );
        void put( char c ) { write( &c, 1 ); }
        void reset();
        sha256 result();

      private:
        struct      impl;
        fc::fwd<impl,112> my;
    };

    template<typename T>
    inline friend T& operator<<( T& ds, const sha256& ep ) {
      ds.write( ep.data(), sizeof(ep) );
      return ds;
    }

    template<typename T>
    inline friend T& operator>>( T& ds, sha256& ep ) {
      ds.read( ep.data(), sizeof(ep) );
      return ds;
    }
    friend sha256 operator << ( const sha256& h1, uint32_t i       );
    friend sha256 operator >> ( const sha256& h1, uint32_t i       );
    friend bool   operator == ( const sha256& h1, const sha256& h2 );
    friend bool   operator != ( const sha256& h1, const sha256& h2 );
    friend sha256 operator ^  ( const sha256& h1, const sha256& h2 );
    friend bool   operator >= ( const sha256& h1, const sha256& h2 );
    friend bool   operator >  ( const sha256& h1, const sha256& h2 ); 
    friend bool   operator <  ( const sha256& h1, const sha256& h2 ); 

    boost::endian::little_uint64_buf_t _hash[4];
};

namespace raw {

   template<typename T>
   inline void pack( T& ds, const sha256& ep, uint32_t _max_depth ) {
      ds << ep;
   }

   template<typename T>
   inline void unpack( T& ds, sha256& ep, uint32_t _max_depth ) {
      ds >> ep;
   }

}

  typedef sha256 uint256;

  class variant;
  void to_variant( const sha256& bi, variant& v, uint32_t max_depth );
  void from_variant( const variant& v, sha256& bi, uint32_t max_depth );

} // fc
namespace std
{
    template<>
    struct hash<fc::sha256>
    {
       size_t operator()( const fc::sha256& s )const
       {
           return  *((size_t*)&s);
       }
    };
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT_TYPENAME( fc::sha256 )
