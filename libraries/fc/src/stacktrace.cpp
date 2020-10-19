/*
 * Copyright (c) 2018-2019 BitShares Blockchain Foundation, and contributors.
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

#include <ostream>
#include <boost/version.hpp>

// only include stacktrace stuff if boost >= 1.65 and not macOS
#if BOOST_VERSION / 100 >= 1065 && !defined(__APPLE__)
#include <signal.h>
#include <fc/log/logger.hpp>
#if defined(__OpenBSD__)
    #define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#endif
#include <boost/stacktrace.hpp>

namespace fc
{

static void segfault_signal_handler(int signum)
{
   ::signal(signum, SIG_DFL);
   std::stringstream ss;
   ss << boost::stacktrace::stacktrace();
   elog(ss.str());
   ::raise(SIGABRT);
}

void print_stacktrace_on_segfault()
{
   ::signal(SIGSEGV, &segfault_signal_handler);
}

void print_stacktrace(std::ostream& out)
{
   out << boost::stacktrace::stacktrace();
}

}
#else
// Stacktrace output requires Boost 1.65 or above.
// Therefore calls to these methods do nothing.
namespace fc
{
void print_stacktrace_on_segfault() {}
void print_stacktrace(std::ostream& out) {}
}

#endif
