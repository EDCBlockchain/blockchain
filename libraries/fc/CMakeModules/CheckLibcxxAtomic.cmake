# Taken from https://chromium.googlesource.com/chromium/llvm-project/libcxx/+/refs/heads/master/cmake/Modules/CheckLibcxxAtomic.cmake
# Apache License v2.0 with LLVM Exceptions

INCLUDE(CheckCXXSourceCompiles)

# Sometimes linking against libatomic is required for atomic ops, if
# the platform doesn't support lock-free atomics.

function(check_cxx_atomics varname)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
  if (${LIBCXX_GCC_TOOLCHAIN})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} --gcc-toolchain=${LIBCXX_GCC_TOOLCHAIN}")
  endif()
  if (CMAKE_C_FLAGS MATCHES -fsanitize OR CMAKE_CXX_FLAGS MATCHES -fsanitize)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fno-sanitize=all")
  endif()
  if (CMAKE_C_FLAGS MATCHES -fsanitize-coverage OR CMAKE_CXX_FLAGS MATCHES -fsanitize-coverage)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fno-sanitize-coverage=edge,trace-cmp,indirect-calls,8bit-counters")
  endif()

  set(OLD_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
  set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})

  check_cxx_source_compiles("
#include <cstdint>
#include <boost/lockfree/queue.hpp>

boost::lockfree::queue<uint32_t*,boost::lockfree::capacity<5>> q;
int main(int, char**) {
  uint32_t* a;
  uint32_t* b;
  q.push(a);
  q.pop(b);
}
" ${varname})
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_INCLUDES ${OLD_CMAKE_REQUIRED_INCLUDES})
endfunction(check_cxx_atomics)

# Perform the check for 64bit atomics without libatomic.
check_cxx_atomics(LIBCXX_HAVE_CXX_ATOMICS_WITHOUT_LIB)
# If not, check if the library exists, and atomics work with it.
if(NOT LIBCXX_HAVE_CXX_ATOMICS_WITHOUT_LIB)
  check_library_exists(atomic __atomic_fetch_add_8 "" LIBCXX_HAS_ATOMIC_LIB)
  if(LIBCXX_HAS_ATOMIC_LIB)
    list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
    check_cxx_atomics(LIBCXX_HAVE_CXX_ATOMICS_WITH_LIB)
    if (NOT LIBCXX_HAVE_CXX_ATOMICS_WITH_LIB)
      message(WARNING "Host compiler must support std::atomic!")
    endif()
  else()
    message(WARNING "Host compiler appears to require libatomic, but cannot find it.")
  endif()
endif()
