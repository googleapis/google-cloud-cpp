# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# A minimal CMakeList.txt showing how to use the Cloud Bigtable C++ client
# library in CMake-based projects.

cmake_minimum_required(VERSION 3.5)
project(google-cloud-cpp-bigtable-example-stock-backtesting CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(google_cloud_cpp_bigtable REQUIRED)

# MSVC requires some additional code to select the correct runtime library
if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# Once the bigtable_client package is found, define new targets.
add_executable(populate_data populate_data_main.cc)
target_link_libraries(
	populate_data
	google-cloud-cpp::bigtable
	absl::strings
	absl::time
	)

set(PROTO ${CMAKE_CURRENT_SOURCE_DIR}/strategy.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO})

add_executable(backtesting backtesting_main.cc ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(
	backtesting
	google-cloud-cpp::bigtable
	absl::strings
	absl::time
	protobuf::libprotobuf
	)

