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

#include "../src/array_hash_set.hpp"

#define BOOST_TEST_MODULE array_hash_set_test
#include <boost/test/unit_test.hpp>

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/vector.hpp>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "../src/environment.hpp"
#include "../src/exceptions.hpp"
#include "../src/mp_integer.hpp"
#include "../src/serialization.hpp"
#include "../src/thread_pool.hpp"
#include "../src/type_traits.hpp"

// Number of tries in random tests.
static const int ntries = 1000;

using namespace piranha;

// NOTE: here we define a custom string class base on std::string that respects nothrow requirements in array_hash_set:
// in the current GCC (4.6) the destructor of std::string does not have nothrow, so we cannot use it.
class custom_string: public std::string
{
	public:
		custom_string() = default;
		custom_string(const custom_string &) = default;
		custom_string(custom_string &&other) noexcept : std::string(std::move(other)) {}
		template <typename... Args>
		custom_string(Args && ... params) : std::string(std::forward<Args>(params)...) {}
		custom_string &operator=(const custom_string &) = default;
		custom_string &operator=(custom_string &&other) noexcept
		{
			std::string::operator=(std::move(other));
			return *this;
		}
		~custom_string() noexcept {}
};

namespace std
{
template <>
struct hash<custom_string>
{
	typedef size_t result_type;
	typedef custom_string argument_type;
	result_type operator()(const argument_type &s) const
	{
		return hash<std::string>{}(s);
	}
};
}

using key_types = boost::mpl::vector<int,integer,custom_string>;

// Number of set items to be created by make_hash_set().
const int N = 10000;

template <typename T>
static inline array_hash_set<T> make_hash_set()
{
	struct lc_func_type
	{
		T operator()(int n) const
		{
			return boost::lexical_cast<T>(n);
		}
	};
	lc_func_type lc_func;
	return array_hash_set<T>(boost::make_transform_iterator(boost::counting_iterator<int>(0),lc_func),
		boost::make_transform_iterator(boost::counting_iterator<int>(N),lc_func)
	);
}

struct range_ctor_tester
{
	template <typename T>
	void operator()(const T &)
	{
		BOOST_CHECK_EQUAL(make_hash_set<T>().size(),unsigned(N));
	}
};

struct copy_ctor_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>()), h_copy(h);
		BOOST_CHECK_EQUAL(h_copy.size(),unsigned(N));
		auto it1 = h.begin();
		for (auto it2 = h_copy.begin(); it2 != h_copy.end(); ++it1, ++it2) {
			BOOST_CHECK_EQUAL(*it1,*it2);
		}
		BOOST_CHECK(it1 == h.end());
	}
};

struct move_ctor_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>()), h_copy(h), h_move(std::move(h));
		BOOST_CHECK_EQUAL(h_copy.size(),unsigned(N));
		BOOST_CHECK_EQUAL(h_move.size(),unsigned(N));
		BOOST_CHECK_EQUAL(h.size(),unsigned(0));
		auto it1 = h_move.begin();
		for (auto it2 = h_copy.begin(); it2 != h_copy.end(); ++it1, ++it2) {
			BOOST_CHECK_EQUAL(*it1,*it2);
		}
		BOOST_CHECK(it1 == h_move.end());
	}
};

struct copy_assignment_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>()), h_copy;
		h_copy = h;
		BOOST_CHECK_EQUAL(h_copy.size(),unsigned(N));
		auto it1 = h.begin();
		for (auto it2 = h_copy.begin(); it2 != h_copy.end(); ++it1, ++it2) {
			BOOST_CHECK_EQUAL(*it1,*it2);
		}
		BOOST_CHECK(it1 == h.end());
	}
};

struct move_assignment_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>()), h_copy(h), h_move;
		h_move = std::move(h);
		BOOST_CHECK_EQUAL(h_copy.size(),unsigned(N));
		BOOST_CHECK_EQUAL(h_move.size(),unsigned(N));
		BOOST_CHECK_EQUAL(h.size(),unsigned(0));
		auto it1 = h_move.begin();
		for (auto it2 = h_copy.begin(); it2 != h_copy.end(); ++it1, ++it2) {
			BOOST_CHECK_EQUAL(*it1,*it2);
		}
		BOOST_CHECK(it1 == h_move.end());
	}
};

struct initializer_list_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h({boost::lexical_cast<T>("1"),boost::lexical_cast<T>("2"),boost::lexical_cast<T>("3"),boost::lexical_cast<T>("4"),boost::lexical_cast<T>("4")});
		BOOST_CHECK_EQUAL(h.size(),unsigned(4));
		for (int i = 1; i <= 4; ++i) {
			BOOST_CHECK(h.find(boost::lexical_cast<T>(i)) != h.end());
		}
	}
};

std::mt19937 rng;
std::uniform_int_distribution<int> dist(0,9);

// Struct that randomly fails on copy.
struct random_failure
{
	random_failure()
	{
		throw;
	}
	random_failure(int n):m_str(boost::lexical_cast<std::string>(n)) {}
	random_failure(const random_failure &rf):m_str(rf.m_str)
	{
		if (!dist(rng)) {
			throw std::runtime_error("fail!");
		}
	}
	random_failure(random_failure &&rf) noexcept :m_str(std::move(rf.m_str)) {}
	~random_failure() noexcept {}
	std::size_t hash() const
	{
		return static_cast<std::size_t>(boost::lexical_cast<int>(m_str));
	}
	bool operator==(const random_failure &rf) const
	{
		return m_str == rf.m_str;
	}
	random_failure &operator=(random_failure &&other) noexcept
	{
		m_str = std::move(other.m_str);
		return *this;
	}
	std::string m_str;
};

namespace std
{
template <>
struct hash<random_failure>
{
	typedef size_t result_type;
	typedef random_failure argument_type;
	result_type operator()(const random_failure &rf) const
	{
		return rf.hash();
	}
};
}

BOOST_AUTO_TEST_CASE(array_hash_set_constructors_test)
{
	environment env;
	// Def ctor.
	array_hash_set<custom_string> ht;
	BOOST_CHECK(ht.begin() == ht.end());
	BOOST_CHECK(ht.empty());
	BOOST_CHECK_EQUAL(ht.size(),unsigned(0));
	BOOST_CHECK_EQUAL(ht.bucket_count(),unsigned(0));
	BOOST_CHECK_THROW(ht.bucket("hello"),zero_division_error);
	// Ctor from number of buckets.
	array_hash_set<custom_string> ht0(0);
	BOOST_CHECK(ht0.bucket_count() == 0);
	BOOST_CHECK(ht0.begin() == ht0.end());
	array_hash_set<custom_string> ht1(1);
	BOOST_CHECK(ht1.bucket_count() >= 1);
	BOOST_CHECK(ht1.begin() == ht1.end());
	array_hash_set<custom_string> ht2(2);
	BOOST_CHECK(ht2.bucket_count() >= 2);
	BOOST_CHECK(ht2.begin() == ht2.end());
	array_hash_set<custom_string> ht3(3);
	BOOST_CHECK(ht3.bucket_count() >= 3);
	BOOST_CHECK(ht3.begin() == ht3.end());
	array_hash_set<custom_string> ht4(4);
	BOOST_CHECK(ht4.bucket_count() >= 4);
	BOOST_CHECK(ht4.begin() == ht4.end());
	array_hash_set<custom_string> ht5(456);
	BOOST_CHECK(ht5.bucket_count() >= 456);
	BOOST_CHECK(ht5.begin() == ht5.end());
	array_hash_set<custom_string> ht6(100001);
	BOOST_CHECK(ht6.bucket_count() >= 100001);
	BOOST_CHECK(ht6.begin() == ht6.end());
	// Range constructor.
	boost::mpl::for_each<key_types>(range_ctor_tester());
	// Copy ctor.
	boost::mpl::for_each<key_types>(copy_ctor_tester());
	// Move ctor.
	boost::mpl::for_each<key_types>(move_ctor_tester());
	// Copy assignment.
	boost::mpl::for_each<key_types>(copy_assignment_tester());
	// Move assignment.
	boost::mpl::for_each<key_types>(move_assignment_tester());
	// Initializer list.
	boost::mpl::for_each<key_types>(initializer_list_tester());
	// Check that requesting too many buckets throws.
	BOOST_CHECK_THROW(ht6 = array_hash_set<custom_string>(std::numeric_limits<std::size_t>::max()),std::bad_alloc);
	// Check unwind on throw.
	// NOTE: prepare table with large number of buckets, so we are sure the first copy of random_failure will be performed
	// in the assignment below.
	array_hash_set<random_failure> ht7(10000);
	for (int i = 0; i < 1000; ++i) {
		ht7.insert(random_failure(i));
	}
	array_hash_set<random_failure> ht8;
	BOOST_CHECK_THROW(ht8 = ht7,std::runtime_error);
}

struct iterator_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>());
		unsigned count = 0;
		for (auto it = h.begin();  it != h.end(); ++it, ++count) {}
		BOOST_CHECK_EQUAL(h.size(),count);
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_iterator_test)
{
	boost::mpl::for_each<key_types>(iterator_tester());
}

struct find_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>()), h_empty;
		BOOST_CHECK(h_empty.find(boost::lexical_cast<T>(0)) == h_empty.end());
		for (int i = 0; i < N; ++i) {
			auto it = h.find(boost::lexical_cast<T>(i));
			BOOST_CHECK(it != h.end());
		}
		BOOST_CHECK(h.find(boost::lexical_cast<T>(N + 1)) == h.end());
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_find_test)
{
	boost::mpl::for_each<key_types>(find_tester());
}

struct insert_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h;
		for (int i = 0; i < N; ++i) {
			auto r1 = h.insert(boost::lexical_cast<T>(i));
			BOOST_CHECK_EQUAL(r1.second,true);
			auto r2 = h.insert(boost::lexical_cast<T>(i));
			BOOST_CHECK_EQUAL(r2.second,false);
			BOOST_CHECK(r2.first == h.find(boost::lexical_cast<T>(i)));
		}
		BOOST_CHECK_EQUAL(h.size(),unsigned(N));
	}
};

// NOTE: this test had a meaning in a previous implementation of array_hash_set, now it is jut a simple
// insertion test.
BOOST_AUTO_TEST_CASE(array_hash_set_insert_test)
{
	// Check insert when the resize operation fails on the first try.
	const std::size_t critical_size = 193;
	struct custom_hash
	{
		std::size_t operator()(std::size_t i) const
		{
			return i;
		}
	};
	custom_hash ch;
	array_hash_set<std::size_t,custom_hash> ht(ch);
	for (std::size_t i = 0; i < critical_size; ++i) {
		BOOST_CHECK_EQUAL(ht.insert(i * critical_size).second,true);
	}
	// Verify insertion of all items.
	for (std::size_t i = 0; i < critical_size; ++i) {
		BOOST_CHECK(ht.find(i * critical_size) != ht.end());
	}
	BOOST_CHECK(ht.size() == critical_size);
	boost::mpl::for_each<key_types>(insert_tester());
}

struct erase_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>());
		for (int i = 0; i < N; ++i) {
			auto r = h.find(boost::lexical_cast<T>(i));
			BOOST_CHECK(r != h.end());
			h.erase(r);
		}
		BOOST_CHECK_EQUAL(h.size(),unsigned(0));
		h = make_hash_set<T>();
		for (auto it = h.begin(); it != h.end();) {
			it = h.erase(it);
		}
		BOOST_CHECK_EQUAL(h.size(),unsigned(0));
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_erase_test)
{
	boost::mpl::for_each<key_types>(erase_tester());
}

struct clear_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h(make_hash_set<T>());
		h.clear();
		BOOST_CHECK_EQUAL(h.size(),unsigned(0));
		BOOST_CHECK_EQUAL(h.bucket_count(),unsigned(0));
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_clear_test)
{
	boost::mpl::for_each<key_types>(clear_tester());
}

struct load_factor_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h;
		BOOST_CHECK(h.load_factor() == 0.);
		array_hash_set<T> i(10);
		BOOST_CHECK_EQUAL(i.load_factor(),0);
		array_hash_set<T> j(make_hash_set<T>());
		BOOST_CHECK(j.load_factor() > 0);
		BOOST_CHECK(j.load_factor() <= 1);
		BOOST_CHECK(h.max_load_factor() > 0);
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_load_factor_test)
{
	boost::mpl::for_each<key_types>(load_factor_tester());
}

struct m_iterators_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h;
		BOOST_CHECK(h._m_begin() == h._m_end());
		h.insert(T());
		BOOST_CHECK(h._m_begin() != h._m_end());
		*h._m_begin() = boost::lexical_cast<T>("42");
		BOOST_CHECK(*h._m_begin() == boost::lexical_cast<T>("42"));
		// Check we can clear and destroy without bad consequences.
		h.clear();
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_m_iterators_test)
{
	boost::mpl::for_each<key_types>(m_iterators_tester());
}

struct rehash_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h;
		BOOST_CHECK(h.bucket_count() == 0u);
		h.rehash(100u);
		BOOST_CHECK(h.bucket_count() >= 100u);
		h.rehash(10u);
		BOOST_CHECK(h.bucket_count() >= 10u);
		h.rehash(1000u);
		BOOST_CHECK(h.bucket_count() >= 1000u);
		h.rehash(0u);
		BOOST_CHECK(h.bucket_count() == 0u);
		h = make_hash_set<T>();
		auto old = h.bucket_count();
		h.rehash(old * 2u);
		BOOST_CHECK(h.bucket_count() >= old * 2u);
		h.rehash(old);
		BOOST_CHECK(h.bucket_count() >= old);
		h = make_hash_set<T>();
		old = h.bucket_count();
		h.rehash(0u);
		BOOST_CHECK(old == h.bucket_count());
		h = array_hash_set<T>(100u);
		h.rehash(0u);
		BOOST_CHECK(h.bucket_count() == 0u);
		h = make_hash_set<T>();
		old = h.bucket_count();
		h.rehash(1000u);
		BOOST_CHECK(h.bucket_count() == old);
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_rehash_test)
{
	boost::mpl::for_each<key_types>(rehash_tester());
}

struct evaluate_sparsity_tester
{
	template <typename T>
	void operator()(const T &)
	{
		array_hash_set<T> h;
		using size_type = typename array_hash_set<T>::size_type;
		BOOST_CHECK((h.evaluate_sparsity() == std::map<size_type,size_type>{}));
		T tmp = T();
		h.insert(tmp);
		BOOST_CHECK((h.evaluate_sparsity() == std::map<size_type,size_type>{{1u,1u}}));
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_evaluate_sparsity_test)
{
	boost::mpl::for_each<key_types>(evaluate_sparsity_tester());
}

struct type_traits_tester
{
	template <typename T>
	void operator()(const T &)
	{
		BOOST_CHECK(is_container_element<array_hash_set<T>>::value);
		BOOST_CHECK((is_instance_of<array_hash_set<T>,array_hash_set>::value));
		BOOST_CHECK(!is_equality_comparable<array_hash_set<T>>::value);
		BOOST_CHECK(!is_addable<array_hash_set<T>>::value);
		BOOST_CHECK(!is_ostreamable<array_hash_set<T>>::value);
	}
};

BOOST_AUTO_TEST_CASE(array_hash_set_type_traits_test)
{
	boost::mpl::for_each<key_types>(type_traits_tester());
}

BOOST_AUTO_TEST_CASE(array_hash_set_mt_test)
{
	thread_pool::resize(4u);
	BOOST_CHECK_THROW(array_hash_set<int>(10000,std::hash<int>(),std::equal_to<int>(),0u),std::invalid_argument);
	array_hash_set<int> h1(100000,std::hash<int>(),std::equal_to<int>(),1u);
	array_hash_set<int> h2(100000,std::hash<int>(),std::equal_to<int>(),2u);
	array_hash_set<int> h3(100000,std::hash<int>(),std::equal_to<int>(),3u);
	array_hash_set<int> h4(100000,std::hash<int>(),std::equal_to<int>(),4u);
	// Try with few buckets.
	array_hash_set<int> h5(1,std::hash<int>(),std::equal_to<int>(),4u);
	array_hash_set<int> h6(2,std::hash<int>(),std::equal_to<int>(),4u);
	array_hash_set<int> h7(3,std::hash<int>(),std::equal_to<int>(),4u);
	array_hash_set<int> h8(4,std::hash<int>(),std::equal_to<int>(),4u);
	// Random testing.
	using size_type = array_hash_set<int>::size_type;
	std::uniform_int_distribution<size_type> size_dist(0u,100000u);
	std::uniform_int_distribution<unsigned> thread_dist(1u,4u);
	for (int i = 0; i < ntries; ++i) {
		auto bcount = size_dist(rng);
		array_hash_set<int> h(bcount,std::hash<int>(),std::equal_to<int>(),thread_dist(rng));
		BOOST_CHECK(h.bucket_count() >= bcount);
		bcount = size_dist(rng);
		h.rehash(bcount,thread_dist(rng));
		BOOST_CHECK(h.bucket_count() >= bcount);
	}
}

BOOST_AUTO_TEST_CASE(array_hash_set_serialization_test)
{
	{
	// Serialize and deserialize hash sets of ints built randomly.
	// Check that the objects have the same size and that every element
	// of one set is also in the other one.
	array_hash_set<int> tmp;
	std::uniform_int_distribution<int> int_dist(std::numeric_limits<int>::min(),
		std::numeric_limits<int>::max());
	std::uniform_int_distribution<unsigned> size_dist(0u,10u);
	for (int i = 0; i < ntries; ++i) {
		array_hash_set<int> h;
		const auto size = size_dist(rng);
		for (auto j = 0u; j < size; ++j) {
			h.insert(int_dist(rng));
		}
		std::stringstream ss;
		{
		boost::archive::text_oarchive oa(ss);
		oa << h;
		}
		{
		boost::archive::text_iarchive ia(ss);
		ia >> tmp;
		}
		BOOST_CHECK(tmp.size() == h.size());
		for (const auto &n: h) {
			BOOST_CHECK(tmp.find(n) != tmp.end());
		}
	}
	}
	{
	// Same with integer.
	array_hash_set<integer> tmp;
	std::uniform_int_distribution<int> int_dist(std::numeric_limits<int>::min(),
		std::numeric_limits<int>::max());
	std::uniform_int_distribution<unsigned> size_dist(0u,10u);
	for (int i = 0; i < ntries; ++i) {
		array_hash_set<integer> h;
		const auto size = size_dist(rng);
		for (auto j = 0u; j < size; ++j) {
			h.insert(integer(int_dist(rng)));
		}
		std::stringstream ss;
		{
		boost::archive::text_oarchive oa(ss);
		oa << h;
		}
		{
		boost::archive::text_iarchive ia(ss);
		ia >> tmp;
		}
		BOOST_CHECK(tmp.size() == h.size());
		for (const auto &n: h) {
			BOOST_CHECK(tmp.find(n) != tmp.end());
		}
	}
	}
}