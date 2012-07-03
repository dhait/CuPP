/*
 * Copyright: See LICENSE file that comes with this distribution
 *
 */

#ifndef CUPP_host_ptr_H
#define CUPP_host_ptr_H

#if defined(__CUDACC__)
#error "Not compatible with CUDA. Don't compile with nvcc."
#endif

#include <string>

// CUDA
#include "cupp/device.h"

#include "boost/shared_ptr.hpp"

namespace cupp {

/**
 * @class host_ptr
 * @author David Hait
 * @platform Host only!
 * @brief This class is a handle to CUDA host page-locked memory.  It has the semantics of a shared_ptr.
 */

template<int Flags>
struct is_mapped
{
	enum { value = ( Flags & cudaHostAllocMapped != 0 ) };
};

template<int Flags>
struct is_portable
{
	enum { value = ( Flags & cudaHostAllocPortable != 0 ) };
};

template<int Flags>
struct is_write_combined
{
	enum { value = ( Flags & cudaHostAllocWriteCombined != 0 ) };
};


template<typename T, unsigned int Flags = 0 >
class host_ptr : public boost::shared_ptr<T>
{
public:
	const static unsigned int flags = Flags;
	const static unsigned int mapped = cudaHostAllocMapped;
	const static unsigned int portable = cudaHostAllocPortable;
	const static unsigned int write_combined = cudaHostAllocWriteCombined;

	class host_deleter
	{
	public:
		void operator()(T * t)
		{
			cupp::host_free(t);
		}
	};

public:
	host_ptr() : boost::shared_ptr<T>(0, host_deleter()) {}
	host_ptr(size_t s) : boost::shared_ptr<T>(
		cupp::host_alloc<T>(s, (unsigned int)flags),
		host_deleter()) {}
};


}

#endif