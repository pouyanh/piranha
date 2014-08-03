/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef PIRANHA_QUADMATH_HPP
#define PIRANHA_QUADMATH_HPP

// Need to include this first in order to check whether quadmath
// support was enabled.
#include "config.hpp"

#if defined(PIRANHA_HAVE_QUADMATH)

#include <iostream>
#include <quadmath.h>
#include <stdexcept>
#include <type_traits>

#include "exceptions.hpp"
#include "math.hpp"
#include "print_coefficient.hpp"

namespace piranha
{

inline namespace literals
{

inline __float128 operator "" _f128(const char *s)
{
	return ::strtoflt128(s,nullptr);
}

}

template <typename T>
struct print_coefficient_impl<T,typename std::enable_if<std::is_same<T,__float128>::value>::type>
{
	std::ostream &operator()(std::ostream &os, const __float128 &cf) const
	{
		// Plenty of buffer.
		char buf[128u];
		// Check that our assumption is correct. This should be converted to
		// a string and passed into the format string really, but for now this
		// will do.
		static_assert(33 == FLT128_DIG,"Invalid value for FLT128_DIG.");
		// NOTE: here we use 34 because constants in quadmath.h are defined with that
		// many digits after the decimal separator. Not sure if the last one could be garbage though.
		const int retval = ::quadmath_snprintf(buf,sizeof(buf),"%.34Qe",cf);
		if (unlikely(retval < 0)) {
			piranha_throw(std::invalid_argument,"quadmath_snprintf() returned an error");
		}
		if (unlikely(static_cast<unsigned>(retval) >= sizeof(buf))) {
			piranha_throw(std::invalid_argument,"quadmath_snprintf() returned a truncated output");
		}
		os << buf;
		return os;
	}
};

namespace detail
{

// Enabler for the __float128 exponentiation method.
template <typename T, typename U>
using pow128_enabler = typename std::enable_if<
		(std::is_same<T,__float128>::value && std::is_same<U,__float128>::value) ||
		(std::is_same<T,__float128>::value && std::is_arithmetic<U>::value) ||
		(std::is_same<U,__float128>::value && std::is_arithmetic<T>::value)
	>::type;

}

namespace math
{

/// Specialisation of the piranha::math::pow() functor for \p __float128.
/**
 * This specialisation is activated when one of the two types is \p __float128 and the other is either
 * \p __float128 or an arithmetic type.
 */
// TODO extension to integer, rational and real arguments.
template <typename T, typename U>
struct pow_impl<T,U,detail::pow128_enabler<T,U>>
{
	/// Call operator.
	/**
	 * The exponentiation will be computed via <tt>powq()</tt>.
	 *
	 * @param[in] x base.
	 * @param[in] y exponent.
	 *
	 * @return \p x to the power of \p y.
	 */
	auto operator()(const T &x, const U &y) const noexcept -> decltype(::powq(x,y))
	{
		return ::powq(x,y);
	}
};

/// Specialisation of the piranha::math::cos() functor for \p __float128.
template <typename T>
struct cos_impl<T,typename std::enable_if<std::is_same<T,__float128>::value>::type>
{
	/// Call operator.
	/**
	 * The cosine will be computed via <tt>cosq()</tt>.
	 * 
	 * @param[in] x argument.
	 * 
	 * @return cosine of \p x.
	 */
	auto operator()(const T &x) const noexcept -> decltype(::cosq(x))
	{
		return ::cosq(x);
	}
};

/// Specialisation of the piranha::math::sin() functor for \p __float128.
template <typename T>
struct sin_impl<T,typename std::enable_if<std::is_same<T,__float128>::value>::type>
{
	/// Call operator.
	/**
	 * The sine will be computed via <tt>sinq()</tt>.
	 * 
	 * @param[in] x argument.
	 * 
	 * @return sine of \p x.
	 */
	auto operator()(const T &x) const noexcept -> decltype(::sinq(x))
	{
		return ::sinq(x);
	}
};


/// Specialisation of the piranha::math::abs() functor for \p __float128.
template <typename T>
struct abs_impl<T,typename std::enable_if<std::is_same<__float128,T>::value>::type>
{
	public:
		/// Call operator.
		/**
		 * The implementation will use the <tt>fabsq()</tt> function.
		 * 
		 * @param[in] x input parameter.
		 * 
		 * @return absolute value of \p x.
		 */
		auto operator()(const T &x) const noexcept -> decltype(::fabsq(x))
		{
			return ::fabsq(x);
		}
};

}

}

#endif

#endif
