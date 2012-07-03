/*
 * Copyright: See LICENSE file that comes with this distribution
 *
 */

#ifndef CUPP_device_ptr_H
#define CUPP_device_ptr_H

#if defined(__CUDACC__)
#error "Not compatible with CUDA. Don't compile with nvcc."
#endif

#include <string>

// CUDA
#include "cupp/device.h"

#include "boost/shared_ptr.hpp"

namespace cupp {


/**
 * @class device_ptr
 * @author David Hait
 * @platform Host only!
 * @brief This class is a handle to CUDA global memory.  It has the semantics of a shared_ptr.
 */
template<typename T>
class device_ptr;


template<typename T>
class device_ptr : public boost::shared_ptr<T>
{
	
public:
	class device_deleter
	{
	public:
		device_deleter(device & d) : device_(d) {}
		void operator()(T * t)
		{
			device_.free(t);
		}
	private:
		device & device_;
	};

	device_ptr(device & dev) : boost::shared_ptr<T>(0, device_deleter(dev)) {}
	device_ptr(device & dev, size_t s) : boost::shared_ptr<T>(dev.malloc<T>(s), device_deleter(dev)) {}
};



}

#endif