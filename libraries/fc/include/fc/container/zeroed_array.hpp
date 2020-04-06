/*
 * Copyright (c) 2019 BitShares Blockchain Foundation, and contributors
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <fc/io/raw_fwd.hpp>

namespace fc {

   template< typename T, size_t N >
   class zero_initialized_array;

   template< size_t N >
   class zero_initialized_array< unsigned char, N > : public std::array< unsigned char, N > {
      public:
         zero_initialized_array() : std::array< unsigned char, N >() { }
   };

   template<typename T,size_t N>
   struct get_typename< zero_initialized_array<T,N> >
   {
      static const char* name()
      {
         static std::string _name = std::string("zero_initialized_array<")
                                    + std::string(fc::get_typename<T>::name())
                                    + "," + fc::to_string(N) + ">";
         return _name.c_str();
      }
   };

   class variant;
   template<size_t N>
   void to_variant( const zero_initialized_array<unsigned char,N>& bi, variant& v, uint32_t max_depth = 1 )
   {
      to_variant( static_cast<const std::array<unsigned char,N>&>( bi ), v, max_depth );
   }
   template<size_t N>
   void from_variant( const variant& v, zero_initialized_array<unsigned char,N>& bi, uint32_t max_depth = 1 )
   {
      from_variant( v, static_cast<std::array<unsigned char,N>&>( bi ), max_depth );
   }

   namespace raw {
      template<typename Stream, size_t N>
      inline void pack( Stream& s, const zero_initialized_array<unsigned char,N>& v, uint32_t _max_depth ) {
         pack( s, static_cast<const std::array<unsigned char,N>&>( v ), _max_depth );
      }
      template<typename Stream, size_t N>
      inline void unpack( Stream& s, zero_initialized_array<unsigned char,N>& v, uint32_t _max_depth ) {
         try {
            unpack( s, static_cast<std::array<unsigned char,N>&>( v ), _max_depth );
         } FC_RETHROW_EXCEPTIONS( warn, "zero_initialized_array<unsigned char,${length}>", ("length",N) )
      }
   }
}
