/*
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#ifndef CUPP_KERNEL_IMPL_test_dirty_H
#define CUPP_KERNEL_IMPL_test_dirty_H

// STD
#include <vector>

// BOOST
#include <boost/type_traits.hpp>

namespace cupp {
namespace kernel_impl {

/**
 * @class test_dirty
 * @author Jens Breitbart
 * @version 0.1
 * @date 23.07.2007
 * @brief Use by cupp::kernel_impl::kernel_launcher_impl to introspec the __global__ function and to detetmine which arguments of the functions are non-const references
 * @warning The is very strictly tied to kernel_launcher_impl; I would not expect any usage beyond this.
 * @note This is a friend class of kernel_launcher_impl, but not defined into the same header to make it easier to extend it and to keep it clear.
 */

template <int arirty>
class test_dirty {
	/**
	 * @param F is the type of the __global__ function to be introspected
	 * @return The vector will have the size of the arity of the __global__ function. @a true at pos 0 means the first parameter is passed as non-const reference.
	 */
	template <typename F>
	static std::vector<bool> dirty ();
};


template <>
class test_dirty<3> {
	template <typename F>
	static std::vector<bool> dirty ();
	
	template <typename T>
	friend class kernel_launcher_impl;
	
	template <typename T>
	friend std::vector<bool> test_dirty<4>::dirty();
};

template <>
class test_dirty<2> {
	template <typename F>
	static std::vector<bool> dirty ();
	
	template <typename T>
	friend class kernel_launcher_impl;
	
	template <typename T>
	friend std::vector<bool> test_dirty<3>::dirty();
};

template <>
class test_dirty<1> {
	template <typename F>
	static std::vector<bool> dirty ();
	
	template <typename T>
	friend class kernel_launcher_impl;
	
	template <typename T>
	friend std::vector<bool> test_dirty<2>::dirty();
};

template <>
class test_dirty<0> {
	template <typename F>
	static std::vector<bool> dirty ();
	
	template <typename T>
	friend class kernel_launcher_impl;
	
	template <typename T>
	friend std::vector<bool> test_dirty<1>::dirty();
};


/*** IMPLEMENTATION ***/

/**
 * @returns @a true if ARG is a non-const reference
 */
template <typename ARG>
inline bool check_arg() {
	if (boost::is_reference<ARG>::value && !is_second_level_const<ARG>::value) {
		return true;
	}
	
	return false;
}

template <typename F>
std::vector<bool> test_dirty<0>::dirty () {
	return std::vector<bool>();
}

template <typename F>
std::vector<bool> test_dirty<1>::dirty () {
	typedef typename boost::function_traits<F>::arg1_type ARG;
	
	std::vector< bool > tmp(test_dirty<0>::dirty<F>());
	tmp.push_back (check_arg<ARG>());
	return tmp;
}

template <typename F>
std::vector<bool> test_dirty<2>::dirty () {
	typedef typename boost::function_traits<F>::arg2_type ARG;
	
	std::vector< bool > tmp(test_dirty<1>::dirty<F>());
	tmp.push_back (check_arg<ARG>());
	return tmp;
}

template <typename F>
std::vector<bool> test_dirty<3>::dirty () {
	typedef typename boost::function_traits<F>::arg3_type ARG;
	
	std::vector< bool > tmp(test_dirty<2>::dirty<F>() );
	tmp.push_back (check_arg<ARG>());
	return tmp;
}


} // kernel_impl
} // cupp

#endif //CUPP_KERNEL_IMPL_real_setup_argument_H
