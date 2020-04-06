/*
 * Copyright (c) 2019 BitShares Blockchain Foundation, and contributors.
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

#ifdef __SIZEOF_INT128__

#include <stdint.h>

namespace fc {

using int128_t  = __int128_t;
using uint128_t = __uint128_t;

inline uint64_t uint128_lo64(const uint128_t x) {
  return static_cast<uint64_t>(x & 0xffffffffffffffffULL);
}
inline uint64_t uint128_hi64(const uint128_t x) {
  return static_cast<uint64_t>( x >> 64 );
}

} // namespace fc

#else // __SIZEOF_INT128__

#include <boost/multiprecision/integer.hpp>

namespace fc {

using boost::multiprecision::int128_t;
using boost::multiprecision::uint128_t;
 
inline uint64_t uint128_lo64(const uint128_t& x) {
  return static_cast<uint64_t>(x & 0xffffffffffffffffULL);
}
inline uint64_t uint128_hi64(const uint128_t& x) {
  return static_cast<uint64_t>( x >> 64 );
}

} // namespace fc

#endif // __SIZEOF_INT128__

namespace fc {

inline uint128_t uint128( const uint64_t hi, const uint64_t lo ) {
   return ( uint128_t(hi) << 64 ) + lo;
}

} // namespace fc
