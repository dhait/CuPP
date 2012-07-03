# Add current directory to the nvcc include line.
CUDA_INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/include )

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/)

INCLUDE_DIRECTORIES( ${GTEST_INCLUDE_DIRS}  ${CMAKE_SOURCE_DIR}/include)

#list all source files here
FILE(GLOB TEST_SRC_FILES *.cpp)

ADD_EXECUTABLE(unit_tests ${TEST_SRC_FILES})

TARGET_LINK_LIBRARIES(unit_tests ${GTEST_BOTH_LIBRARIES} ${CUDA_LIBRARIES})

ADD_TEST(unit_tests unit_tests)
		
