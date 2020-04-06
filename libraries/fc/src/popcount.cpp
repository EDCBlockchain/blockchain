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

#include <fc/popcount.hpp>

namespace fc {

#if _MSC_VER || __GNUC__ || __clang__
#else
uint8_t popcount( uint64_t v )
{
   // Public Domain code taken from http://graphics.stanford.edu/~seander/bithacks.html
   uint8_t c;
   static const int S[] = {1, 2, 4, 8, 16, 32};
   static const uint64_t B[] = {0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F,
                                0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF, 0x00000000FFFFFFFF };

   c = v - ((v >> 1) & B[0]);
   c = ((c >> S[1]) & B[1]) + (c & B[1]);
   c = ((c >> S[2]) + c) & B[2];
   c = ((c >> S[3]) + c) & B[3];
   c = ((c >> S[4]) + c) & B[4];
   return ((c >> S[5]) + c) & B[5];
}
#endif

uint8_t popcount( const uint128_t& v )
{
    return popcount( static_cast<uint64_t>(v >> 64) )
                     + popcount( static_cast<uint64_t>(v & 0xffffffffffffffffULL) );
}

} // namespace fc
