#include <boost/test/unit_test.hpp>

#include <fc/exception/exception.hpp>

#include <type_traits>

struct reflect_test_base {
   int x = 1;
   char y = 'a';
};
struct reflect_test_derived : reflect_test_base {
   double z = 3.14;
};
struct reflect_layer_1 { reflect_test_base b; int32_t n; };
struct reflect_layer_2 { reflect_layer_1 l1; reflect_test_derived d; };
struct reflect_layer_3 { reflect_layer_2 l2; int32_t i; };

FC_REFLECT( reflect_test_base, (x)(y) );
FC_REFLECT_DERIVED( reflect_test_derived, (reflect_test_base), (z) );
FC_REFLECT( reflect_layer_1, (b)(n) );
FC_REFLECT( reflect_layer_2, (l1)(d) );
FC_REFLECT( reflect_layer_3, (l2)(i) );

BOOST_AUTO_TEST_SUITE( fc_reflection )

BOOST_AUTO_TEST_CASE( reflection_static_tests )
{
   // These are all compile-time tests, nothing actually happens here at runtime
   using base_reflection = fc::reflector<reflect_test_base>;
   using derived_reflection = fc::reflector<reflect_test_derived>;
   static_assert(fc::typelist::length<base_reflection::members>() == 2, "");
   static_assert(fc::typelist::length<derived_reflection::members>() == 3, "");
   static_assert(fc::typelist::at<derived_reflection::members, 0>::is_derived, "");
   static_assert(std::is_same<fc::typelist::at<derived_reflection::members, 0>::field_container,
                              reflect_test_base>::value, "");
   static_assert(fc::typelist::at<derived_reflection::members, 1>::is_derived, "");
   static_assert(std::is_same<fc::typelist::at<derived_reflection::members, 1>::field_container,
                              reflect_test_base>::value, "");
   static_assert(fc::typelist::at<derived_reflection::members, 2>::is_derived == false, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 0, 1>,
                              fc::typelist::list<int>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 0, 2>,
                              fc::typelist::list<int, bool>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 0, 3>,
                              fc::typelist::list<int, bool, char>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 1, 3>,
                              fc::typelist::list<bool, char>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 2, 3>,
                              fc::typelist::list<char>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 1, 2>,
                              fc::typelist::list<bool>>::value, "");
   static_assert(std::is_same<fc::typelist::slice<fc::typelist::list<int, bool, char>, 1>,
                              fc::typelist::list<bool, char>>::value, "");
   static_assert(std::is_same<fc::typelist::make_sequence<0>, fc::typelist::list<>>::value, "");
   static_assert(std::is_same<fc::typelist::make_sequence<1>,
                              fc::typelist::list<std::integral_constant<size_t, 0>>>::value, "");
   static_assert(std::is_same<fc::typelist::make_sequence<2>,
                              fc::typelist::list<std::integral_constant<size_t, 0>,
                                                 std::integral_constant<size_t, 1>>>::value, "");
   static_assert(std::is_same<fc::typelist::make_sequence<3>,
                              fc::typelist::list<std::integral_constant<size_t, 0>,
                                                 std::integral_constant<size_t, 1>,
                                                 std::integral_constant<size_t, 2>>>::value, "");
   static_assert(std::is_same<fc::typelist::zip<fc::typelist::list<>, fc::typelist::list<>>,
                              fc::typelist::list<>>::value, "");
   static_assert(std::is_same<fc::typelist::zip<fc::typelist::list<bool>, fc::typelist::list<char>>,
                              fc::typelist::list<fc::typelist::list<bool, char>>>::value, "");
   static_assert(std::is_same<fc::typelist::zip<fc::typelist::list<int, bool>, fc::typelist::list<char, double>>,
                              fc::typelist::list<fc::typelist::list<int, char>,
                                                 fc::typelist::list<bool, double>>>::value, "");
   static_assert(std::is_same<fc::typelist::index<fc::typelist::list<>>, fc::typelist::list<>>::value, "");
   static_assert(std::is_same<fc::typelist::index<fc::typelist::list<int, bool, char, double>>,
                              fc::typelist::list<fc::typelist::list<std::integral_constant<size_t, 0>, int>,
                                                 fc::typelist::list<std::integral_constant<size_t, 1>, bool>,
                                                 fc::typelist::list<std::integral_constant<size_t, 2>, char>,
                                                 fc::typelist::list<std::integral_constant<size_t, 3>, double>>
                 >::value, "");
}

BOOST_AUTO_TEST_CASE( typelist_dispatch_test )
{
   using list = fc::typelist::list<float, bool, char>;
   auto get_name = [](auto t) -> std::string { return fc::get_typename<typename decltype(t)::type>::name(); };
   BOOST_CHECK_EQUAL(fc::typelist::runtime::dispatch(list(), 0ul, get_name), "float");
   BOOST_CHECK_EQUAL(fc::typelist::runtime::dispatch(list(), 1ul, get_name), "bool");
   BOOST_CHECK_EQUAL(fc::typelist::runtime::dispatch(list(), 2ul, get_name), "char");
}

// Helper template to use fc::typelist::at without a comma, for macro friendliness
template<typename T> struct index_from { template<std::size_t idx> using at = fc::typelist::at<T, idx>; };
BOOST_AUTO_TEST_CASE( reflection_get_test )
{ try {
   reflect_test_derived derived;
   reflect_test_base& base = derived;

   using base_reflector = fc::reflector<reflect_test_base>;
   using derived_reflector = fc::reflector<reflect_test_derived>;

   BOOST_CHECK(index_from<base_reflector::members>::at<0>::get(base) == 1);
   BOOST_CHECK(index_from<base_reflector::members>::at<1>::get(base) == 'a');

   fc::typelist::at<base_reflector::members, 0>::get(base) = 5;
   fc::typelist::at<base_reflector::members, 1>::get(base) = 'q';

   BOOST_CHECK(index_from<base_reflector::members>::at<0>::get(base) == 5);
   BOOST_CHECK(index_from<base_reflector::members>::at<1>::get(base) == 'q');

   BOOST_CHECK(index_from<derived_reflector::members>::at<0>::get(derived) == 5);
   BOOST_CHECK(index_from<derived_reflector::members>::at<1>::get(derived) == 'q');
   BOOST_CHECK(index_from<derived_reflector::members>::at<2>::get(derived) == 3.14);

   fc::typelist::at<derived_reflector::members, 1>::get(derived) = 'X';

   BOOST_CHECK(index_from<base_reflector::members>::at<1>::get(base) == 'X');

   reflect_layer_3 l3;
   BOOST_CHECK(index_from<index_from<index_from<index_from<fc::reflector<reflect_layer_3>::members>::at<0>
               ::reflector::members>::at<0>::reflector::members>::at<0>::reflector::members>::at<1>::get(l3.l2.l1.b)
               == 'a');
   BOOST_CHECK(index_from<index_from<index_from<fc::reflector<reflect_layer_3>::members>::at<0>::reflector::members>
               ::at<1>::reflector::members>::at<1>::get(l3.l2.d) == 'a');
   BOOST_CHECK(index_from<index_from<index_from<fc::reflector<reflect_layer_3>::members>::at<0>::reflector::members>
               ::at<1>::reflector::members>::at<2>::get(l3.l2.d) == 3.14);
} FC_CAPTURE_LOG_AND_RETHROW( (0) ) }

BOOST_AUTO_TEST_SUITE_END()
