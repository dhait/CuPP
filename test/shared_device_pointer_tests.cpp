#include <iostream>
#include "gtest/gtest.h"
#include "cupp/device.h"
#include "cupp/runtime.h"
#include "cupp/shared_device_pointer.h"
#include "cupp/device_ptr.h"
#include "cupp/host_ptr.h"

TEST(shared_device_pointer_tests, basic)
{
	cupp::device d;
	cupp::shared_device_pointer<int> p1(d.malloc<int>(1));

	int * t = p1.get();
	

	ASSERT_EQ(p1.useCount(), 1);
	
	cupp::shared_device_pointer<int> p2 = p1;

	ASSERT_EQ(p2.useCount(), 2);

	p1.reset(0);

	ASSERT_EQ(p2.useCount(), 1);
}


struct mytest
{
	int i;
	int j;
};

TEST(shared_device_pointer_tests, device_ptr)
{
	cupp::device d(1);
	cupp::device_ptr<mytest>  m(d, 10);
	cupp::device_ptr<mytest> m2 = m;
	ASSERT_EQ(m2.use_count(), 2);
	mytest * t = m2.get();   // bad

	m2.reset();
	m.reset();

	ASSERT_EQ(m.use_count(), 0);
	ASSERT_EQ(m2.use_count(), 0);
	// t has already been deleted!
	ASSERT_THROW(d.free(t), cupp::exception::cuda_runtime_error);
}


TEST(shared_device_pointer_tests, host_ptr)
{
	cupp::host_ptr<mytest>  m(10);
	cupp::host_ptr<mytest> m2 = m;
	ASSERT_EQ(m2.use_count(), 2);
	mytest * t = m2.get();   // bad idea

	m2.reset();
	m.reset();

	ASSERT_EQ(m.use_count(), 0);
	ASSERT_EQ(m2.use_count(), 0);

	// t has already been deleted!
	ASSERT_THROW(cupp::host_free(t), cupp::exception::cuda_runtime_error);
}

TEST(shared_device_pointer_tests, host_ptr_mapped)
{
	cupp::host_ptr<mytest, cupp::host_ptr<mytest>::mapped> m(10);
	ASSERT_EQ(cupp::get_host_flags(m.get()), cupp::host_ptr<mytest>::mapped);

}







