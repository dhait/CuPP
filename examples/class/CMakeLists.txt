# Add current directory to the nvcc include line.
CUDA_INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} )

# Link cuda code in a library to something else.
CUDA_ADD_LIBRARY(class_kernel kernel_kernel.cu )

#list all source files here
ADD_EXECUTABLE(class_example kernel.cpp)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(class_example class_kernel ${CUDA_LIBRARY})

# set minimum cmake version
cmake_minimum_required(VERSION 2.4)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
