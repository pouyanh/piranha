/* Copyright 2009-2016 Francesco Biscani (bluescarni@gmail.com)

This file is part of the Piranha library.

The Piranha library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 3 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The Piranha library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the Piranha library.  If not,
see https://www.gnu.org/licenses/. */

#ifndef PIRANHA_TYPE_TRAITS_HPP
#define PIRANHA_TYPE_TRAITS_HPP

/** \file type_traits.hpp
 * \brief Type traits.
 *
 * This header contains general-purpose type traits classes.
 */

#include <cstdarg>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "config.hpp"
#include "detail/sfinae_types.hpp"

namespace piranha
{

inline namespace impl
{

// http://en.cppreference.com/w/cpp/types/void_t
template <typename... Ts>
struct make_void {
    typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

// http://en.cppreference.com/w/cpp/experimental/is_detected
template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
struct detector {
    using value_t = std::false_type;
    using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, void_t<Op<Args...>>, Op, Args...> {
    using value_t = std::true_type;
    using type = Op<Args...>;
};

// http://en.cppreference.com/w/cpp/experimental/nonesuch
struct nonesuch {
    nonesuch() = delete;
    ~nonesuch() = delete;
    nonesuch(nonesuch const &) = delete;
    void operator=(nonesuch const &) = delete;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
using detected_t = typename detector<nonesuch, void, Op, Args...>::type;

// http://en.cppreference.com/w/cpp/types/conjunction
template <class...>
struct conjunction : std::true_type {
};

template <class B1>
struct conjunction<B1> : B1 {
};

template <class B1, class... Bn>
struct conjunction<B1, Bn...> : std::conditional<B1::value != false, conjunction<Bn...>, B1>::type {
};

// http://en.cppreference.com/w/cpp/types/disjunction
template <class...>
struct disjunction : std::false_type {
};

template <class B1>
struct disjunction<B1> : B1 {
};

template <class B1, class... Bn>
struct disjunction<B1, Bn...> : std::conditional<B1::value != false, B1, disjunction<Bn...>>::type {
};

// http://en.cppreference.com/w/cpp/types/negation
template <class B>
struct negation : std::integral_constant<bool, !B::value> {
};

// std::index_sequence and std::make_index_sequence implementation for C++11. These are available
// in the std library in C++14. Implementation taken from:
// http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence
template <std::size_t... Ints>
struct index_sequence {
    using type = index_sequence;
    using value_type = std::size_t;
    static constexpr std::size_t size() noexcept
    {
        return sizeof...(Ints);
    }
};

template <class Sequence1, class Sequence2>
struct merge_and_renumber;

template <std::size_t... I1, std::size_t... I2>
struct merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
    : index_sequence<I1..., (sizeof...(I1) + I2)...> {
};

template <std::size_t N>
struct make_index_sequence
    : merge_and_renumber<typename make_index_sequence<N / 2>::type, typename make_index_sequence<N - N / 2>::type> {
};

template <>
struct make_index_sequence<0> : index_sequence<> {
};

template <>
struct make_index_sequence<1> : index_sequence<0> {
};

template <typename T, typename F, std::size_t... Is>
void apply_to_each_item(T &&t, const F &f, index_sequence<Is...>)
{
    (void)std::initializer_list<int>{0, (void(f(std::get<Is>(std::forward<T>(t)))), 0)...};
}

// Tuple for_each(). Execute the functor f on each element of the input Tuple.
// https://isocpp.org/blog/2015/01/for-each-arg-eric-niebler
// https://www.reddit.com/r/cpp/comments/2tffv3/for_each_argumentsean_parent/
// https://www.reddit.com/r/cpp/comments/33b06v/for_each_in_tuple/
template <class Tuple, class F>
void tuple_for_each(Tuple &&t, const F &f)
{
    apply_to_each_item(std::forward<Tuple>(t), f,
                       make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>{});
}

// Some handy aliases.
template <typename T>
using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template <typename T>
using unref_t = typename std::remove_reference<T>::type;

template <typename T>
using addlref_t = typename std::add_lvalue_reference<T>::type;

template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template <typename T>
using is_nonconst_rvalue_ref
    = std::integral_constant<bool, std::is_rvalue_reference<T>::value
                                       && !std::is_const<typename std::remove_reference<T>::type>::value>;

template <typename T, typename U, typename Derived>
struct arith_tt_helper {
    static const bool value = std::is_same<decltype(Derived::test(*(unref_t<T> *)nullptr, *(unref_t<U> *)nullptr)),
                                           detail::sfinae_types::yes>::value;
};
}

/// Addable type trait.
/**
 * Will be \p true if objects of type \p T can be added to objects of type \p U using the binary addition operator.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator+(const Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 is_addable<int>::value == true;
 is_addable<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class is_addable : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_addable<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t + u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_addable>::value;
};

template <typename T, typename U>
const bool is_addable<T, U>::value;

/// In-place addable type trait.
/**
 * Will be \p true if objects of type \p U can be added in-place to objects of type \p T.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator+=(Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 is_addable_in_place<int>::value == true;
 is_addable_in_place<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class is_addable_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_addable_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t += u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_addable_in_place>::value;
};

template <typename T, typename U>
const bool is_addable_in_place<T, U>::value;

/// Subtractable type trait.
/**
 * @see piranha::is_addable.
 */
template <typename T, typename U = T>
class is_subtractable : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_subtractable<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t - u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_subtractable>::value;
};

template <typename T, typename U>
const bool is_subtractable<T, U>::value;

/// In-place subtractable type trait.
/**
 * @see piranha::is_addable_in_place.
 */
template <typename T, typename U = T>
class is_subtractable_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_subtractable_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t -= u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_subtractable_in_place>::value;
};

template <typename T, typename U>
const bool is_subtractable_in_place<T, U>::value;

/// Multipliable type trait.
/**
 * @see piranha::is_addable.
 */
template <typename T, typename U = T>
class is_multipliable : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_multipliable<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t * u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_multipliable>::value;
};

template <typename T, typename U>
const bool is_multipliable<T, U>::value;

/// In-place multipliable type trait.
/**
 * @see piranha::is_addable_in_place.
 */
template <typename T, typename U = T>
class is_multipliable_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_multipliable_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t *= u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_multipliable_in_place>::value;
};

template <typename T, typename U>
const bool is_multipliable_in_place<T, U>::value;

/// Divisible type trait.
/**
 * @see piranha::is_addable.
 */
template <typename T, typename U = T>
class is_divisible : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_divisible<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t / u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_divisible>::value;
};

template <typename T, typename U>
const bool is_divisible<T, U>::value;

/// In-place divisible type trait.
/**
 * @see piranha::is_addable_in_place.
 */
template <typename T, typename U = T>
class is_divisible_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, is_divisible_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t /= u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, is_divisible_in_place>::value;
};

template <typename T, typename U>
const bool is_divisible_in_place<T, U>::value;

/// Equality-comparable type trait.
/**
 * This type trait is \p true if instances if type \p T can be compared for equality and inequality to instances of
 * type \p U. The operators must be non-mutable (i.e., implemented using pass-by-value or const
 * references) and must return a type implicitly convertible to \p bool.
 */
template <typename T, typename U = T>
class is_equality_comparable : detail::sfinae_types
{
    typedef typename std::decay<T>::type Td;
    typedef typename std::decay<U>::type Ud;
    template <typename T1, typename U1>
    static auto test1(const T1 &t, const U1 &u) -> decltype(t == u);
    static no test1(...);
    template <typename T1, typename U1>
    static auto test2(const T1 &t, const U1 &u) -> decltype(t != u);
    static no test2(...);
    static const bool implementation_defined
        = std::is_convertible<decltype(test1(std::declval<Td>(), std::declval<Ud>())), bool>::value
          && std::is_convertible<decltype(test2(std::declval<Td>(), std::declval<Ud>())), bool>::value;

public:
    /// Value of the type trait.
    static const bool value = implementation_defined;
};

// Static init.
template <typename T, typename U>
const bool is_equality_comparable<T, U>::value;

/// Less-than-comparable type trait.
/**
 * This type trait is \p true if instances of type \p T can be compared to instances of
 * type \p U using the less-than operator. The operator must be non-mutable (i.e., implemented using pass-by-value or
 * const
 * references) and must return a type implicitly convertible to \p bool.
 */
template <typename T, typename U = T>
class is_less_than_comparable : detail::sfinae_types
{
    typedef typename std::decay<T>::type Td;
    typedef typename std::decay<U>::type Ud;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t < u);
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = std::is_convertible<decltype(test(std::declval<Td>(), std::declval<Ud>())), bool>::value;
};

// Static init.
template <typename T, typename U>
const bool is_less_than_comparable<T, U>::value;

/// Greater-than-comparable type trait.
/**
 * This type trait is \p true if instances of type \p T can be compared to instances of
 * type \p U using the greater-than operator. The operator must be non-mutable (i.e., implemented using pass-by-value or
 * const
 * references) and must return a type implicitly convertible to \p bool.
 */
template <typename T, typename U = T>
class is_greater_than_comparable : detail::sfinae_types
{
    typedef typename std::decay<T>::type Td;
    typedef typename std::decay<U>::type Ud;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t > u);
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = std::is_convertible<decltype(test(std::declval<Td>(), std::declval<Ud>())), bool>::value;
};

// Static init.
template <typename T, typename U>
const bool is_greater_than_comparable<T, U>::value;

/// Enable \p noexcept checks.
/**
 * This type trait, to be specialised with the <tt>std::enable_if</tt> mechanism, enables or disables
 * \p noexcept checks in the piranha::is_container_element type trait. This type trait should be used
 * only with legacy pre-C++11 classes that do not support \p noexcept: by specialising this trait to
 * \p false, the piranha::is_container_element trait will disable \p noexcept checks and it will thus be
 * possible to use \p noexcept unaware classes as container elements and series coefficients.
 */
template <typename T, typename = void>
struct enable_noexcept_checks {
    /// Default value of the type trait.
    static const bool value = true;
};

// Static init.
template <typename T, typename Enable>
const bool enable_noexcept_checks<T, Enable>::value;

/// Type trait for well-behaved container elements.
/**
 * The type trait will be true if all these conditions hold:
 *
 * - \p T is default-constructible,
 * - \p T is copy-constructible,
 * - \p T has nothrow move semantics,
 * - \p T is nothrow-destructible.
 *
 * If the piranha::enable_noexcept_checks trait is \p false for \p T, then the nothrow checks will be discarded.
 */
template <typename T>
struct is_container_element {
    // NOTE: here we do not require copy assignability as in our containers we always implement
    // copy-assign as copy-construct + move for exception safety reasons.
    /// Value of the type trait.
    static const bool value = std::is_default_constructible<T>::value && std::is_copy_constructible<T>::value
                              && (!enable_noexcept_checks<T>::value
                                  || (std::is_nothrow_destructible<T>::value
// The Intel compiler has troubles with the noexcept versions of these two type traits.
#if defined(PIRANHA_COMPILER_IS_INTEL)
                                      && std::is_move_constructible<T>::value && std::is_move_assignable<T>::value
#else
                                      && std::is_nothrow_move_constructible<T>::value
                                      && std::is_nothrow_move_assignable<T>::value
#endif
                                      ));
};

template <typename T>
const bool is_container_element<T>::value;

inline namespace impl
{

// Detection of ostreamable types.
template <typename T>
using ostreamable_t = decltype(std::declval<std::ostream &>() << std::declval<const T &>());
}

/// Type trait for classes that can be output-streamed.
/**
 * This type trait will be \p true if instances of type \p T can be directed to
 * instances of \p std::ostream via the insertion operator. The operator must have a signature
 * compatible with
 * @code
 * std::ostream &operator<<(std::ostream &, const T &)
 * @endcode
 */
template <typename T>
class is_ostreamable
{
    static const bool implementation_defined = std::is_same<detected_t<ostreamable_t, T>, std::ostream &>::value;

public:
    /// Value of the type trait.
    static const bool value = implementation_defined;
};

template <typename T>
const bool is_ostreamable<T>::value;

namespace detail
{

template <typename T>
class is_hashable_impl : detail::sfinae_types
{
    typedef typename std::decay<T>::type Td;
    template <typename T1>
    static auto test(const T1 &t) -> decltype(std::declval<const std::hash<T1> &>()(t));
    static no test(...);

public:
    static const bool value = std::is_same<decltype(test(std::declval<Td>())), std::size_t>::value;
};
}

/// Hashable type trait.
/**
 * This type trait will be \p true if the decay type of \p T is hashable, \p false otherwise.
 *
 * A type \p T is hashable when supplied with a specialisation of \p std::hash which:
 * - has a call operator complying to the interface specified by the C++ standard,
 * - satisfies piranha::is_container_element.
 *
 * Note that depending on the implementation of the default \p std::hash class, using this type trait with
 * a type which does not provide a specialisation for \p std::hash could result in a compilation error
 * (e.g., if the unspecialised \p std::hash includes a \p false \p static_assert).
 */
// NOTE: when we remove the is_container_element check we might need to make sure that std::hash is def ctible,
// depending on how we use it. Check.
template <typename T, typename = void>
class is_hashable
{
public:
    /// Value of the type trait.
    static const bool value = false;
};

template <typename T>
class is_hashable<T, typename std::enable_if<detail::is_hashable_impl<T>::value>::type>
{
    typedef typename std::decay<T>::type Td;
    typedef std::hash<Td> hasher;

public:
    static const bool value = is_container_element<hasher>::value;
};

template <typename T, typename Enable>
const bool is_hashable<T, Enable>::value;

template <typename T>
const bool is_hashable<T, typename std::enable_if<detail::is_hashable_impl<T>::value>::type>::value;

namespace detail
{

template <typename, typename, typename = void>
struct is_function_object_impl {
    template <typename... Args>
    struct tt {
        static const bool value = false;
    };
};

template <typename T, typename ReturnType>
struct is_function_object_impl<T, ReturnType, typename std::enable_if<std::is_class<T>::value>::type> {
    template <typename... Args>
    struct tt : detail::sfinae_types {
        template <typename U>
        static auto test(U &f) -> decltype(f(std::declval<Args>()...));
        static no test(...);
        static const bool value = std::is_same<decltype(test(*(T *)nullptr)), ReturnType>::value;
    };
};
}

/// Function object type trait.
/**
 * This type trait will be true if \p T is a function object returning \p ReturnType and taking
 * \p Args as arguments. That is, the type trait will be \p true if the following conditions are met:
 * - \p T is a class,
 * - \p T is equipped with a call operator returning \p ReturnType and taking \p Args as arguments.
 *
 * \p T can be const qualified (in which case the call operator must be also const in order for the type trait to be
 * satisfied).
 */
template <typename T, typename ReturnType, typename... Args>
class is_function_object
{
public:
    /// Value of the type trait.
    static const bool value = detail::is_function_object_impl<T, ReturnType>::template tt<Args...>::value;
};

template <typename T, typename ReturnType, typename... Args>
const bool is_function_object<T, ReturnType, Args...>::value;

/// Type trait to detect hash function objects.
/**
 * \p T is a hash function object for \p U if the following requirements are met:
 * - \p T is a function object with const call operator accepting as input const \p U and returning \p std::size_t,
 * - \p T satisfies piranha::is_container_element.
 *
 * The decay type of \p U is considered in this type trait.
 */
template <typename T, typename U, typename = void>
class is_hash_function_object
{
public:
    /// Value of the type trait.
    static const bool value = false;
};

template <typename T, typename U>
class is_hash_function_object<T, U, typename std::enable_if<is_function_object<
                                        typename std::add_const<T>::type, std::size_t,
                                        typename std::decay<U>::type const &>::value>::type>
{
    typedef typename std::decay<U>::type Ud;

public:
    static const bool value = is_container_element<T>::value;
};

template <typename T, typename U, typename Enable>
const bool is_hash_function_object<T, U, Enable>::value;

template <typename T, typename U>
const bool is_hash_function_object<T, U, typename std::enable_if<is_function_object<
                                             typename std::add_const<T>::type, std::size_t,
                                             typename std::decay<U>::type const &>::value>::type>::value;

/// Type trait to detect equality function objects.
/**
 * \p T is an equality function object for \p U if the following requirements are met:
 * - \p T is a function object with const call operator accepting as input two const \p U and returning \p bool,
 * - \p T satisfies piranha::is_container_element.
 *
 * The decay type of \p U is considered in this type trait.
 */
template <typename T, typename U, typename = void>
class is_equality_function_object
{
public:
    /// Value of the type trait.
    static const bool value = false;
};

template <typename T, typename U>
class is_equality_function_object<T, U,
                                  typename std::enable_if<is_function_object<
                                      typename std::add_const<T>::type, bool, typename std::decay<U>::type const &,
                                      typename std::decay<U>::type const &>::value>::type>
{
public:
    static const bool value = is_container_element<T>::value;
};

template <typename T, typename U, typename Enable>
const bool is_equality_function_object<T, U, Enable>::value;

template <typename T, typename U>
const bool
    is_equality_function_object<T, U, typename std::enable_if<is_function_object<
                                          typename std::add_const<T>::type, bool, typename std::decay<U>::type const &,
                                          typename std::decay<U>::type const &>::value>::type>::value;
}

/// Macro to test if class has type definition.
/**
 * This macro will declare a template struct parametrized over one type \p T and called <tt>has_typedef_type_name</tt>,
 * whose static const bool member \p value will be \p true if \p T contains a \p typedef called \p type_name, false
 * otherwise.
 *
 * For instance:
 * \code
 * PIRANHA_DECLARE_HAS_TYPEDEF(foo_type);
 * struct foo
 * {
 * 	typedef int foo_type;
 * };
 * struct bar {};
 * \endcode
 * \p has_typedef_foo_type<foo>::value will be true and \p has_typedef_foo_type<bar>::value will be false.
 *
 * The decay type of the template argument is considered by the class defined by the macro.
 */
#define PIRANHA_DECLARE_HAS_TYPEDEF(type_name)                                                                         \
    template <typename PIRANHA_DECLARE_HAS_TYPEDEF_ARGUMENT>                                                           \
    class has_typedef_##type_name : piranha::detail::sfinae_types                                                      \
    {                                                                                                                  \
        using Td_ = typename std::decay<PIRANHA_DECLARE_HAS_TYPEDEF_ARGUMENT>::type;                                   \
        template <typename T_>                                                                                         \
        static auto test(const T_ *) -> decltype(std::declval<typename T_::type_name>(), void(), yes());               \
        static no test(...);                                                                                           \
                                                                                                                       \
    public:                                                                                                            \
        static const bool value = std::is_same<yes, decltype(test((Td_ *)nullptr))>::value;                            \
    }

/// Macro for static type trait checks.
/**
 * This macro will check via a \p static_assert that the template type trait \p tt provides a \p true \p value.
 * The variadic arguments are interpreted as the template arguments of \p tt.
 */
#define PIRANHA_TT_CHECK(tt, ...)                                                                                      \
    static_assert(tt<__VA_ARGS__>::value, "type trait check failure -> " #tt "<" #__VA_ARGS__ ">")

namespace piranha
{

namespace detail
{

template <typename T, typename... Args>
struct min_int_impl {
    using next = typename min_int_impl<Args...>::type;
    static_assert((std::is_unsigned<T>::value && std::is_unsigned<next>::value && std::is_integral<next>::value)
                      || (std::is_signed<T>::value && std::is_signed<next>::value && std::is_integral<next>::value),
                  "The type trait's arguments must all be (un)signed integers.");
    using type = typename std::conditional<(std::numeric_limits<T>::max() < std::numeric_limits<next>::max()
                                            && (std::is_unsigned<T>::value
                                                || std::numeric_limits<T>::min() > std::numeric_limits<next>::min())),
                                           T, next>::type;
};

template <typename T>
struct min_int_impl<T> {
    static_assert(std::is_integral<T>::value, "The type trait's arguments must all be (un)signed integers.");
    using type = T;
};

template <typename T, typename... Args>
struct max_int_impl {
    using next = typename max_int_impl<Args...>::type;
    static_assert((std::is_unsigned<T>::value && std::is_unsigned<next>::value && std::is_integral<next>::value)
                      || (std::is_signed<T>::value && std::is_signed<next>::value && std::is_integral<next>::value),
                  "The type trait's arguments must all be (un)signed integers.");
    using type = typename std::conditional<(std::numeric_limits<T>::max() > std::numeric_limits<next>::max()
                                            && (std::is_unsigned<T>::value
                                                || std::numeric_limits<T>::min() < std::numeric_limits<next>::min())),
                                           T, next>::type;
};

template <typename T>
struct max_int_impl<T> {
    static_assert(std::is_integral<T>::value, "The type trait's arguments must all be (un)signed integers.");
    using type = T;
};
}

/// Detect narrowest integer type
/**
 * This type alias requires \p T and \p Args (if any) to be all signed or unsigned integer types.
 * It will be defined as the input type with the narrowest numerical range.
 */
template <typename T, typename... Args>
using min_int = typename detail::min_int_impl<T, Args...>::type;

/// Detect widest integer type
/**
 * This type alias requires \p T and \p Args (if any) to be all signed or unsigned integer types.
 * It will be defined as the input type with the widest numerical range.
 */
template <typename T, typename... Args>
using max_int = typename detail::max_int_impl<T, Args...>::type;

namespace detail
{

#if !defined(PIRANHA_DOXYGEN_INVOKED)

// Detect the availability of std::iterator_traits on type It, plus a couple more requisites from the
// iterator concept.
// NOTE: this needs also the is_swappable type trait, but this seems to be difficult to implement in C++11. Mostly
// because
// it seems that:
// - we cannot detect a specialised std::swap (so if it is not specialised, it will pick the default implementation
//   which could fail in the implementation without giving hints in the prototype),
// - it's tricky to fulfill the requirement that swap has to be called unqualified (cannot use 'using std::swap' within
// a decltype()
//   SFINAE, might be doable with automatic return type deduction for regular functions in C++14?).
template <typename It>
struct has_iterator_traits {
    using it_tags = std::tuple<std::input_iterator_tag, std::output_iterator_tag, std::forward_iterator_tag,
                               std::bidirectional_iterator_tag, std::random_access_iterator_tag>;
    PIRANHA_DECLARE_HAS_TYPEDEF(difference_type);
    PIRANHA_DECLARE_HAS_TYPEDEF(value_type);
    PIRANHA_DECLARE_HAS_TYPEDEF(pointer);
    PIRANHA_DECLARE_HAS_TYPEDEF(reference);
    PIRANHA_DECLARE_HAS_TYPEDEF(iterator_category);
    using i_traits = std::iterator_traits<It>;
    static const bool value = has_typedef_reference<i_traits>::value && has_typedef_value_type<i_traits>::value
                              && has_typedef_pointer<i_traits>::value && has_typedef_difference_type<i_traits>::value
                              && has_typedef_iterator_category<i_traits>::value && std::is_copy_constructible<It>::value
                              && std::is_copy_assignable<It>::value && std::is_destructible<It>::value;
};

template <typename It>
const bool has_iterator_traits<It>::value;

// TMP to check if a type is convertible to a type in the tuple.
template <typename T, typename Tuple, std::size_t I = 0u, typename Enable = void>
struct convertible_type_in_tuple {
    static_assert(I < std::numeric_limits<std::size_t>::max(), "Overflow error.");
    static const bool value = std::is_convertible<T, typename std::tuple_element<I, Tuple>::type>::value
                              || convertible_type_in_tuple<T, Tuple, I + 1u>::value;
};

template <typename T, typename Tuple, std::size_t I>
struct convertible_type_in_tuple<T, Tuple, I, typename std::enable_if<I == std::tuple_size<Tuple>::value>::type> {
    static const bool value = false;
};

template <typename T, typename = void>
struct is_iterator_impl {
    static const bool value = false;
};

// NOTE: here the correct condition is the commented one, as opposed to the first one appearing. However, it seems like
// there are inconsistencies between the commented condition and the definition of many output iterators in the standard
// library:
//
// http://stackoverflow.com/questions/23567244/apparent-inconsistency-in-iterator-requirements
//
// Until this is clarified, it is probably better to keep this workaround.
template <typename T>
struct is_iterator_impl<T,
                        typename std::
                            enable_if</*std::is_same<typename
                                         std::iterator_traits<T>::reference,decltype(*std::declval<T &>())>::value &&*/
                                      // That is the one that would need to be replaced with the one above. Just check
                                      // that operator*() is defined.
                                      std::is_same<decltype(*std::declval<T &>()),
                                                   decltype(*std::declval<T &>())>::value
                                      && std::is_same<decltype(++std::declval<T &>()), T &>::value
                                      && has_iterator_traits<T>::value &&
                                      // NOTE: here we used to have type_in_tuple, but it turns out Boost.iterator
                                      // defines its own set of tags derived from the standard
                                      // ones. Hence, check that the category can be converted to one of the standard
                                      // categories. This should not change anything for std iterators,
                                      // and just enable support for Boost ones.
                                      convertible_type_in_tuple<typename std::iterator_traits<T>::iterator_category,
                                                                typename has_iterator_traits<T>::it_tags>::value>::
                                type> {
    static const bool value = true;
};

#endif
}

/// Iterator type trait.
/**
 * This type trait will be \p true if the decay type of \p T satisfies the compile-time requirements of an iterator (as
 * defined by the C++ standard),
 * \p false otherwise.
 */
template <typename T>
struct is_iterator {
    /// Value of the type trait.
    static const bool value = detail::is_iterator_impl<typename std::decay<T>::type>::value;
};

template <typename T>
const bool is_iterator<T>::value;

namespace detail
{

#if !defined(PIRANHA_DOXYGEN_INVOKED)

template <typename T, typename = void>
struct is_input_iterator_impl {
    static const bool value = false;
};

template <typename T, typename = void>
struct arrow_operator_type {
};

template <typename T>
struct arrow_operator_type<T, typename std::enable_if<std::is_pointer<T>::value>::type> {
    using type = T;
};

template <typename T>
struct arrow_operator_type<T, typename std::enable_if<std::is_same<
                                  typename arrow_operator_type<decltype(std::declval<T &>().operator->())>::type,
                                  typename arrow_operator_type<decltype(std::declval<T &>().operator->())>::type>::
                                                          value>::type> {
    using type = typename arrow_operator_type<decltype(std::declval<T &>().operator->())>::type;
};

template <typename T>
struct is_input_iterator_impl<T,
                              typename std::
                                  enable_if<is_iterator_impl<T>::value && is_equality_comparable<T>::value
                                            && std::is_convertible<decltype(*std::declval<T &>()),
                                                                   typename std::iterator_traits<T>::value_type>::value
                                            && std::is_same<decltype(++std::declval<T &>()), T &>::value
                                            && std::is_same<decltype((void)std::declval<T &>()++),
                                                            decltype((void)std::declval<T &>()++)>::value
                                            && std::is_convertible<decltype(*std::declval<T &>()++),
                                                                   typename std::iterator_traits<T>::value_type>::value
                                            &&
                                            // NOTE: here we know that the arrow op has to return a pointer, if
                                            // implemented correctly, and that the syntax
                                            // it->m must be equivalent to (*it).m. This means that, barring differences
                                            // in reference qualifications,
                                            // it-> and *it must return the same thing.
                                            std::is_same<
                                                typename std::remove_reference<decltype(
                                                    *std::declval<typename arrow_operator_type<T>::type>())>::type,
                                                typename std::remove_reference<decltype(*std::declval<T &>())>::type>::
                                                value
                                            &&
                                            // NOTE: here the usage of is_convertible guarantees we catch both iterators
                                            // higher in the type hierarchy and
                                            // the Boost versions of standard iterators as well.
                                            std::is_convertible<typename std::iterator_traits<T>::iterator_category,
                                                                std::input_iterator_tag>::value>::type> {
    static const bool value = true;
};

#endif
}

/// Input iterator type trait.
/**
 * This type trait will be \p true if the decay type of \p T satisfies the compile-time requirements of an input
 * iterator (as defined by the C++ standard),
 * \p false otherwise.
 */
template <typename T>
struct is_input_iterator {
    /// Value of the type trait.
    static const bool value = detail::is_input_iterator_impl<typename std::decay<T>::type>::value;
};

template <typename T>
const bool is_input_iterator<T>::value;

namespace detail
{

template <typename T, typename = void>
struct is_forward_iterator_impl {
    static const bool value = false;
};

template <typename T>
struct is_forward_iterator_impl<T,
                                typename std::
                                    enable_if<is_input_iterator_impl<T>::value
                                              && std::is_default_constructible<T>::value
                                              && (std::is_same<typename std::iterator_traits<T>::value_type &,
                                                               typename std::iterator_traits<T>::reference>::value
                                                  || std::is_same<typename std::iterator_traits<T>::value_type const &,
                                                                  typename std::iterator_traits<T>::reference>::value)
                                              && std::is_convertible<decltype(std::declval<T &>()++), const T &>::value
                                              && std::is_same<decltype(*std::declval<T &>()++),
                                                              typename std::iterator_traits<T>::reference>::value
                                              && std::is_convertible<
                                                     typename std::iterator_traits<T>::iterator_category,
                                                     std::forward_iterator_tag>::value>::type> {
    static const bool value = true;
};
}

/// Forward iterator type trait.
/**
 * This type trait will be \p true if the decay type of \p T satisfies the compile-time requirements of a forward
 * iterator (as defined by the C++ standard),
 * \p false otherwise.
 */
template <typename T>
struct is_forward_iterator {
    /// Value of the type trait.
    static const bool value = detail::is_forward_iterator_impl<typename std::decay<T>::type>::value;
};

template <typename T>
const bool is_forward_iterator<T>::value;

namespace detail
{

#if !defined(PIRANHA_DOXYGEN_INVOKED)
template <typename T>
constexpr T safe_abs_sint_impl(T cur_p = T(1), T cur_n = T(-1))
{
    return (cur_p > std::numeric_limits<T>::max() / T(2) || cur_n < std::numeric_limits<T>::min() / T(2))
               ? cur_p
               : safe_abs_sint_impl(static_cast<T>(cur_p * 2), static_cast<T>(cur_n * 2));
}

// Determine, for the signed integer T, a value n, power of 2, such that it is safe to take -n.
template <typename T>
struct safe_abs_sint {
    static_assert(std::is_integral<T>::value && std::is_signed<T>::value, "T must be a signed integral type.");
    static const T value = safe_abs_sint_impl<T>();
};

template <typename T>
const T safe_abs_sint<T>::value;
#endif

// A simple true type-trait that can be used inside enable_if with T a decltype() expression
// subject to SFINAE. It is similar to the proposed void_t type (in C++14, maybe?).
template <typename T>
struct true_tt {
    static const bool value = true;
};

template <typename T>
const bool true_tt<T>::value;
}

/// Detect the availability of <tt>std::begin()</tt> and <tt>std::end()</tt>.
/**
 * This type trait will be \p true if all the following conditions are fulfilled:
 *
 * - <tt>std::begin()</tt> and <tt>std::end()</tt> can be called on instances of \p T, yielding the type \p It,
 * - \p It is an input iterator.
 *
 * Any reference qualification in \p T is ignored by this type trait.
 */
template <typename T>
class has_begin_end : detail::sfinae_types
{
    using Td = typename std::remove_reference<T>::type;
    template <typename T1>
    static auto test1(T1 &t) -> decltype(std::begin(t));
    static no test1(...);
    template <typename T1>
    static auto test2(T1 &t) -> decltype(std::end(t));
    static no test2(...);

public:
    /// Value of the type trait.
    static const bool value
        = is_input_iterator<decltype(test1(std::declval<Td &>()))>::value
          && is_input_iterator<decltype(test2(std::declval<Td &>()))>::value
          && std::is_same<decltype(test1(std::declval<Td &>())), decltype(test2(std::declval<Td &>()))>::value;
};

template <typename T>
const bool has_begin_end<T>::value;

/// Left-shift type trait.
/**
 * Will be \p true if objects of type \p T can be left-shifted by objects of type \p U.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator<<(const Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 has_left_shift<int>::value == true;
 has_left_shift<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class has_left_shift : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, has_left_shift<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t << u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, has_left_shift>::value;
};

template <typename T, typename U>
const bool has_left_shift<T, U>::value;

/// Right-shift type trait.
/**
 * Will be \p true if objects of type \p T can be right-shifted by objects of type \p U.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator>>(const Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 has_right_shift<int>::value == true;
 has_right_shift<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class has_right_shift : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, has_right_shift<T, U>>;
    template <typename T1, typename U1>
    static auto test(const T1 &t, const U1 &u) -> decltype(t >> u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, has_right_shift>::value;
};

template <typename T, typename U>
const bool has_right_shift<T, U>::value;

/// In-place left-shift type trait.
/**
 * Will be \p true if objects of type \p T can be left-shifted in-place by objects of type \p U.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator<<=(Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 has_left_shift_in_place<int>::value == true;
 has_left_shift_in_place<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class has_left_shift_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, has_left_shift_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t <<= u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, has_left_shift_in_place>::value;
};

template <typename T, typename U>
const bool has_left_shift_in_place<T, U>::value;

/// In-place right-shift type trait.
/**
 * Will be \p true if objects of type \p T can be right-shifted in-place by objects of type \p U.
 *
 * This type trait will strip \p T and \p U of reference qualifiers, and it will test the operator in the form
 @code
 operator<<=(Td &, const Ud &)
 @endcode
 * where \p Td and \p Ud are \p T and \p U after the removal of reference qualifiers. E.g.:
 @code
 has_right_shift_in_place<int>::value == true;
 has_right_shift_in_place<int,std::string>::value == false;
 @endcode
 */
template <typename T, typename U = T>
class has_right_shift_in_place : detail::sfinae_types
{
    friend struct arith_tt_helper<T, U, has_right_shift_in_place<T, U>>;
    template <typename T1, typename U1>
    static auto test(T1 &t, const U1 &u) -> decltype(t >>= u, void(), yes());
    static no test(...);

public:
    /// Value of the type trait.
    static const bool value = arith_tt_helper<T, U, has_right_shift_in_place>::value;
};

template <typename T, typename U>
const bool has_right_shift_in_place<T, U>::value;

/// Detect if type has exact ring operations.
/**
 * This type trait should be specialised to \p true if the decay type of \p T supports exact
 * addition, subtraction and multiplication.
 */
template <typename T, typename = void>
struct has_exact_ring_operations {
    /// Value of the type trait.
    /**
     * The default implementation will set the value to \p false.
     */
    static const bool value = false;
};

template <typename T, typename Enable>
const bool has_exact_ring_operations<T, Enable>::value;

/// Detect if type can be returned from a function.
template <typename T>
struct is_returnable {
private:
    static const bool implementation_defined
        = std::is_destructible<T>::value
          && (std::is_copy_constructible<T>::value || std::is_move_constructible<T>::value);

public:
    /// Value of the type trait.
    /**
     * The type trait will be true if \p T is destructible and copy or move
     * constructible.
     */
    static const bool value = implementation_defined;
};

template <typename T>
const bool is_returnable<T>::value;

/// Detect types that can be used as mapped values in associative containers.
/**
 * This type trait is intended to detect whether \p T supports typical operations that can be performed
 * on the mapped values in associative containers.
 * Specifically, this trait will be \p true if all the following conditions hold:
 * - \p T is default constructible,
 * - \p T is copy constructible and assignable,
 * - \p T is move constructible and assignable,
 * - \p T is destructible.
 *
 * Otherwise, the value of this trait will be \p false.
 */
template <typename T>
struct is_mappable {
private:
    static const bool implementation_defined
        = std::is_default_constructible<T>::value && std::is_destructible<T>::value
          && std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value
          && std::is_move_constructible<T>::value && std::is_move_assignable<T>::value;

public:
    /// Value of the type trait.
    static const bool value = implementation_defined;
};

template <typename T>
const bool is_mappable<T>::value;

/// Detect if zero is a multiplicative absorber.
/**
 * This type trait, defaulting to \p true, establishes if the zero element of the type \p T is a multiplicative
 * absorber. That is, if for any value \f$ x \f$ of type \p T the relation
 * \f[
 * x \times 0 = 0
 * \f]
 * holds, then this type trait should be \p true.
 *
 * This type trait requires \p T to satsify piranha::is_multipliable, after removal of cv/reference qualifiers.
 * This type trait can be specialised via \p std::enable_if.
 */
template <typename T, typename = void>
struct zero_is_absorbing {
private:
    PIRANHA_TT_CHECK(is_multipliable, uncvref_t<T>);

public:
    /// Value of the type trait.
    static const bool value = true;
};

template <typename T, typename Enable>
const bool zero_is_absorbing<T, Enable>::value;

inline namespace impl
{

template <typename T>
using fp_zero_is_absorbing_enabler = enable_if_t<std::is_floating_point<uncvref_t<T>>::value
                                                 && (std::numeric_limits<uncvref_t<T>>::has_quiet_NaN
                                                     || std::numeric_limits<uncvref_t<T>>::has_signaling_NaN)>;
}

/// Specialisation of piranha::zero_is_absorbing for floating-point types.
/**
 * \note
 * This specialisation is enabled if \p T, after the removal of cv/reference qualifiers, is a C++ floating-point type
 * supporting NaN.
 *
 * In the presence of NaN, a floating-point zero is not the multiplicative absorbing element.
 *
 * This type traits requires \p T to satsify piranha::is_multipliable, after the removal of cv/reference qualifiers.
 */
template <typename T>
struct zero_is_absorbing<T, fp_zero_is_absorbing_enabler<T>> {
private:
    PIRANHA_TT_CHECK(is_multipliable, uncvref_t<T>);

public:
    /// Value of the type trait.
    static const bool value = false;
};

template <typename T>
const bool zero_is_absorbing<T, fp_zero_is_absorbing_enabler<T>>::value;
}

#endif
