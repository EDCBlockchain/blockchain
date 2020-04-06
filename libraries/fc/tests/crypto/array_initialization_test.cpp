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
#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/rand.hpp>

#include <string.h>

static void check_null_key()
{
   fc::ecc::public_key_data key1;
   fc::ecc::public_key_data key2;
   unsigned char zeroes[33];
   static_assert( key1.size() == sizeof(zeroes), "Wrong array size!" );
   memset( zeroes, 0, sizeof(zeroes) );
   BOOST_CHECK( !memcmp( key1.data(), zeroes, sizeof(zeroes) ) );
   BOOST_CHECK( !memcmp( key2.data(), zeroes, sizeof(zeroes) ) );

   // now "pollute" the keys for the next invocation
   key1 = fc::ecc::private_key::generate().get_public_key();
   for( unsigned char c = 0; c < key2.size(); c++ )
   {
      key2[c] = c ^ 17;
      zeroes[c] = c ^ 47;
   }

   // ...and use them to prevent the compiler from optimizing the pollution away.
   wlog( "Key1: ${k}", ("k",fc::ecc::public_key::to_base58(key1)) );
   wlog( "Key2: ${k}", ("k",fc::ecc::public_key::to_base58(key2)) );
}

BOOST_AUTO_TEST_SUITE(fc_crypto)

BOOST_AUTO_TEST_CASE(array_init_test)
{
   check_null_key();
   check_null_key();
   {
      char junk[128];
      fc::rand_bytes( junk, sizeof(junk) );
   }
   check_null_key();
}

BOOST_AUTO_TEST_SUITE_END()
