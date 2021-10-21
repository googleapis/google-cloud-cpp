# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# A minimal CMakeList.txt showing how to use the Cloud Bigtable C++ client
# library in CMake-based projects.

cmake_minimum_required(VERSION 3.5)
project(google-cloud-cpp-pubsub-quickstart CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(google_cloud_cpp_pubsub REQUIRED)

# MSVC requires some additional code to select the correct runtime library
if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# Once the pubsub_client package is found, define new targets.
add_executable(quickstart quickstart.cc)
target_link_libraries(quickstart google-cloud-cpp::pubsub)
