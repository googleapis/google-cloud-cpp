# Copyright 2019 Google LLC
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

# A minimal CMakeList.txt showing how to use the Google Cloud Storage C++ client
# library in CMake-based projects.

cmake_minimum_required(VERSION 3.10...3.24)
project(google-cloud-cpp-storage-quickstart CXX)

find_package(google_cloud_cpp_storage REQUIRED)

# MSVC requires some additional code to select the correct runtime library
if (VCPKG_TARGET_TRIPLET MATCHES "-static$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# Once the packages are found, define the targets.
add_executable(quickstart quickstart.cc)
target_link_libraries(quickstart google-cloud-cpp::storage)

find_package(google_cloud_cpp_storage_grpc CONFIG)
if (NOT google_cloud_cpp_storage_grpc_FOUND)
    message("GCS+gRPC plugin is disabled, skipping its quickstarts")
    return()
endif ()

# TODO(#13857) - switch to `google-cloud-cpp::storage_grpc` when the CI builds
# are ready
add_executable(quickstart_grpc EXCLUDE_FROM_ALL quickstart_grpc.cc)
target_link_libraries(quickstart_grpc
                      google-cloud-cpp::experimental-storage_grpc)

add_executable(quickstart_async EXCLUDE_FROM_ALL quickstart_async.cc)
target_link_libraries(quickstart_async
                      google-cloud-cpp::experimental-storage_grpc)
