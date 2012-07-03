#include <iostream>
#include "gtest/gtest.h"
#include "cupp/device.h"



TEST(device_tests, basic)
{

	// Tesla C2050 tests

	cupp::device d(0);

	ASSERT_GE(d.device_count(), 0);
	ASSERT_STREQ(d.name(), "Tesla C2050");
	ASSERT_EQ(d.major(), 2);
	ASSERT_EQ(d.warp_size(), 32);
}

TEST(device_tests, context)
{
	cupp::device * d1 = new cupp::device(0);
	cupp::device * d2(d1);
	ASSERT_EQ(d2->name(), d1->name());
}

TEST(device_tests, malloc)
{
	cupp::device d(0);
	int * i = d.malloc<int>(10);
	d.free(i);

	cupp::device d2(0);
	i = d.malloc<int>(10);
	// try to free memory on the same physical device (same thread)
	ASSERT_NO_THROW(d2.free(i));

	cupp::device d3(1);
	i = d.malloc<int>(10);
	// try to free memory on a different device
	ASSERT_THROW(d3.free(i), cupp::exception::cuda_runtime_error);
}

TEST(device_tests, find)
{
	cupp::device d(cupp::device::find(2));
	ASSERT_STREQ(d.name(), "Tesla C2050");

	ASSERT_THROW(cupp::device::find(3), cupp::exception::no_supporting_device);
}