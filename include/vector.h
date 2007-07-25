/*
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#ifndef CUPP_vector_H
#define CUPP_vector_H

#if defined(__CUDACC__)
#error Not compatible with CUDA. Don't compile with nvcc.
#endif

// CUPP
#include "cupp_common.h"
#include "cupp_runtime.h"
#include "kernel_type_binding.h"
#include "shared_device_pointer.h"
#include "device.h"
#include "memory1d.h"

#include "deviceT/vector.h"

#include "exception/cuda_runtime_error.h"
#include "exception/memory_access_violation.h"

// STD
#include <cstddef> // Include std::size_t
#include <algorithm> // Include std::swap
#include <vector>

// CUDA
#include <cuda_runtime.h>


namespace cupp {

// Just used to force the user to configure and get a device.
class device;

// create kernel call bindings
template <typename T>
class kernel_host_type<cupp::deviceT::vector<T> > {
	public:
		typedef cupp::vector<T> type;
};

template <typename T>
class kernel_device_type < cupp::vector<T> > {
	public:
		typedef cupp::deviceT::vector<T> type;
};


/**
 * @class vector
 * @author Jens Breitbart
 * @version 0.1
 * @date 24.07.2007
 * @platform Host only
 * @brief A std::vector wrapper, which can be transfered to the device incl. lazy memory copying.
 * @todo iterators can break everything ... need to write our own iterator here
 */

template< typename T >
class vector {
	public: /*** TYPEDEFS  ***/
		typedef typename std::vector<T>::size_type               size_type;
		typedef typename std::vector<T>::value_type              value_type;
		//typedef typename std::vector<T>::iterator                iterator;
		typedef typename std::vector<T>::const_iterator          const_iterator;
		typedef typename std::vector<T>::const_reverse_iterator  const_reverse_iterator;

		typedef typename kernel_device_type < vector<T> >::type  device_type;

	public: /***  The proxy class  ***/
	
		class element_proxy {
			private: /***  Data elements  ***/
				const size_type at_;
				vector<T>& vector_;
				
			public: /***  Constructors & Destructors  ***/
				element_proxy(const size_type at, vector<T>& vector) :
					at_(at), vector_(vector) {}

			public: /***  Operators  ***/
				operator T() const {
					vector_.update_host();
					return vector_.data_[at_];
				}
				
				element_proxy& operator=(const element_proxy &rhs) {
					rhs.vector_.update_host();
					vector_.update_host();
					vector_.host_changes_ = true;
					vector_.data_[at_]=rhs.vector_.data_[ rhs.at_];
					return *this;
				}
				
				
				element_proxy& operator=(const T& rhs) {
					vector_.update_host();
					vector_.host_changes_ = true;
					vector_.data_[at_] = rhs;
					return *this;
				}
		};
	
		class iterator {
			private: /***  Data elements  ***/
				typename std::vector<T>::iterator i_;
				vector<T>& vector_;
				
			public: /***  Constructors & Destructors  ***/
				iterator(const typename std::vector<T>::iterator i, vector<T>& vector) :
					i_(i), vector_(vector) {}

			public: /***  Operators  ***/
				operator typename std::vector<T>::iterator() const {
					vector_.update_host();
					return i_;
				}

				const T& operator* () const {
					vector_.update_host();
					return *i_;
				}
				
				T& operator* () {
					vector_.update_host();
					vector_.host_changes_ = true;
					return *i_;
				}
				
				const T& operator-> () const {
					vector_.update_host();
					return *i_;
				}
				
				T& operator-> () {
					vector_.update_host();
					vector_.host_changes_ = true;
					return *i_;
				}
				
				void operator++ () {
					++i_;
				}
				
				void operator-- () {
					--i_;
				}

				bool operator!= (const iterator &other) {
					return i_ != other.i_;
				}

				bool operator== (const iterator &other) {
					return i_ == other.i_;
				}
		};
		
		typedef          std::reverse_iterator<iterator>         reverse_iterator;

	public: /***  CONSTRUCTORS AND DESTRUCTORS  ***/
	
		/**
		 * Default constructor
		 */
		vector() : data_(), host_changes_(true), device_changes_(false), proxy_invalid_(false), memory_(0), device_proxy_(0) {}

		/**
		 * Copy constructor
		 */
		vector( const vector& c ) : host_changes_(true), device_changes_(false), proxy_invalid_(false), memory_(0), device_proxy_(0) {
			c.update_host();
			data_(c);
		}

		/**
		 * Creates a vector with @a num elements with the value @a val
		 */
		vector( size_type num, const T& val = T() ) : data_(num, val), host_changes_(true), device_changes_(false), proxy_invalid_(false), memory_(0), device_proxy_(0) {}

		/**
		 * Fills the vector with the data defined by @a start and @a end
		 */
		template <typename input_iterator>
		vector( input_iterator start, input_iterator end ) : data_(start, end), host_changes_(true), device_changes_(false), proxy_invalid_(false), memory_(0), device_proxy_(0) {}

		/**
		 * The destructor
		 */
		~vector() {
			delete memory_;
			delete device_proxy_;
		}


	public: /***  OPERATORS  ***/
		element_proxy operator[]( size_type index ) {
			return element_proxy (index, *this);
		}

		/**
		 * Access the element with @a index inside our vector
		 */
		const T& operator[]( size_type index ) const {
			update_host();
			return data_[index];
		}

		/**
		 * Assign another vector to our vector
		 */
		vector& operator=(const vector& c2) {
			c2.update_host();
			data_ = c2.data_;
			host_changes_ = true;

			// our data have been overwritten ... ignore all old data on the device
			device_changes_ = false;

			return *this;
		}

	public: /***  NORMAL FUNCTIONS  ***/
		void assign( size_type num, const T& val ) {
			data_.assign (num, val);
			
			host_changes_ = true;
			// our data have been overwritten ... ignore all old data on the device
			device_changes_ = false;
		}

		template <typename input_iterator>
		void assign( input_iterator start, input_iterator end ) {
			data_.assign (start, end);
			
			host_changes_ = true;
			// our data have been overwritten ... ignore all old data on the device
			device_changes_ = false;
		}

		element_proxy at( size_type loc ) {
			data_.at(loc);
			return element_proxy (index, *this);
		}
		
		const T& at( size_type loc ) const {
			update_host();
			return data_.at(loc);
		}
		
		element_proxy back() {
			return (data_.size()-1, *this);
		}
		
		const T& back() const {
			update_host();
			return data_.back();
		}

		iterator begin() {
			update_host();
			host_changes_ = true;
			return iterator (data_.begin(), *this);
		}
		
		const_iterator begin() const {
			return data_.begin();
		}

		size_type capacity() const {
			return data_.capacity();
		}

		void clear() {
			data_.clear();
			host_changes_ = true;
			device_changes_ = false;
		}

		bool empty() const {
			return data_.empty();
		}

		iterator end() {
			update_host();
			host_changes_ = true;
			return iterator (data_.end(), *this);
		}
		
		const_iterator end() const {
			update_host();
			return data_.end();
		}
		
		iterator erase( iterator loc ) {
			update_host();
			host_changes_ = true;
			return iterator (data_.erase(loc), *this);
		}
		
		iterator erase( iterator start, iterator end ) {
			update_host();
			host_changes_ = true;
			return iterator (data_.erase(start, end), *this);
		}
		
		element_proxy front() {
			return element_proxy(0, *this);
		}
		
		const T& front() const {
			update_host();
			return data_.front();
		}

		iterator insert( iterator loc, const T& val ) {
			update_host();
			host_changes_ = true;
			return iterator (data_.insert(loc, val), *this);
		}
		
		void insert( iterator loc, size_type num, const T& val ) {
			update_host();
			host_changes_ = true;
			data_.insert(loc, num, val);
		}

		template <typename input_iterator>
		void insert( iterator loc, input_iterator start, input_iterator end ) {
			update_host();
			host_changes_ = true;
			data_.insert(loc, start, end);
		}

		size_type max_size() const {
			return data_.max_size();
		}

		void pop_back() {
			update_host();
			data_.pop_back();
			host_changes_ = true;
		}

		void push_back( const T& val ) {
			update_host();
			data_.push_back(val);
			host_changes_ = true;
		}

		reverse_iterator rbegin() {
			update_host();
			host_changes_ = true;
			return data_.rbegin();
		}

		const_reverse_iterator rbegin() const {
			update_host();
			return data_.rbegin();
		}

		reverse_iterator rend() {
			update_host();
			host_changes_ = true;
			return data_.rend();
		}
		
		const_reverse_iterator rend() const {
			update_host();
			return data_.rend();
		}

		void reserve( size_type size ) {
			data_.reserve(size);
		}

		void resize( size_type num, const T& val = T() ) {
			update_host();
			data_resize (num, val);
			host_changes_ = true;
		}

		size_type size() const {
			return data_.size();
		}

		void swap( vector<T>& from ) {
			update_host();
			
			from.update_host();
			data_.swap(from.data);
			
			host_changes_ = true;
			from.host_changes_ = true;
		}

	public: /*** CUPP FUNCTIONALITY  ***/

		/**
		 * @brief This function is called by the kernel_call_traits
		 * @return A on the device useable memory1d reference
		 */
		device_type get_host_based_device_copy(const device &d) const {
			update_device(d);

			return device_type(size(), memory_->cuda_pointer().get());
		}

		/**
		 * @brief This function is called by the kernel_call_traits
		 * @return A on the device useable memory1d reference
		 */
		shared_device_pointer< device_type > get_device_based_device_copy(const device &d) const {

			device_type returnee = get_host_based_device_copy(d);
			
			if (proxy_invalid_) {
				delete device_proxy_;
				device_proxy_ = new memory1d<device_type> (d, &returnee, 1);
				proxy_invalid_ = false;
			}

			return device_proxy_->cuda_pointer();
		}

		/**
		 * @brief This function is called by the kernel_call_traits
		 */
		void dirty (const device &d, shared_device_pointer< device_type > device_copy) const {
			device_changes_ = true;
		}

		/**
		 * If there is newer data on the device, this function will update the host data with it
		 */
		void update_host() const {
			if (device_changes_) {
				assert(!host_changes_);
				
				memory_ -> copy_to_host (&data_[0]);
				
				device_changes_ = false;
			}
		}

		/**
		 * If there is newer data on the host, this function will update the device data with it
		 */
		void update_device(const device &d) const {
		
			// changes on the host side or we are executed on a new device
			if (host_changes_ || &d != device_ptr_) {
				assert (!device_changes_);
				// we don't have any space on the device, or the we have more/less space on the device than we neeed
				// or we are executed on a new device
				if (memory_ == 0 || memory_ -> size()!=data_.size() || &d != device_ptr_) {
					// free the memory
					delete memory_;
					
					// get new memory and copy the data
					memory_ = new memory1d<T>(d, &data_[0], data_.size() );

					// we need to create a new proxy because our memory has a new address
					proxy_invalid_ = true;
				} else {
					// copy the data to the device
					memory_ -> copy_to_device (&data_[0]);
				}

				device_ptr_ = const_cast<device*> (&d);
				host_changes_ = false;
			}
		}
		

	private:
		/**
		 * Our real vector :-)
		 */
		mutable std::vector<T> data_;
		
		/**
		 * true means, data has been changed on the host and data on the device is out of date
		 */
		mutable bool host_changes_;

		/**
		 * true means, data has been changed on the device and data on the host is out of date
		 */
		mutable bool device_changes_;

		/**
		 * True means we have to recreate our proxy
		 */
		mutable bool proxy_invalid_;

		/**
		 * Our device memory :-)
		 */
		mutable memory1d<T> *memory_;

		/**
		 * The proxy on our device
		 */
		mutable memory1d<device_type> *device_proxy_;

		/**
		 * A pointer to the device on which our data is stored
		 */
		mutable device *device_ptr_;
}; // class vector

template <typename T1, typename T2>
bool operator==(const vector<T1>& c1, const vector<T2>& c2) {
	c1.update_host();
	c2.update_host();
	return c1.data_ == c2.data_;
}

template <typename T1, typename T2>
bool operator!=(const vector<T1>& c1, const vector<T2>& c2) {
	return !(c1 == c2);
}

template <typename T1, typename T2>
bool operator<(const vector<T1>& c1, const vector<T2>& c2) {
	c1.update_host();
	c2.update_host();
	return c1.data_ < c2.data_; 
}

template <typename T1, typename T2>
bool operator>(const vector<T1>& c1, const vector<T2>& c2) {
	c1.update_host();
	c2.update_host();
	return c1.data_ > c2.data_;
}

template <typename T1, typename T2>
bool operator<=(const vector<T1>& c1, const vector<T2>& c2) {
	return !(c1 > c2);
}

template <typename T1, typename T2>
bool operator>=(const vector<T1>& c1, const vector<T2>& c2) {
	return !(c1 < c2);
}


} // namespace cupp

#endif