# ~~~
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

cmake_minimum_required(VERSION 3.6)
project(verify-exported-targets CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake can use pkg-config to find dependencies. We recommend using CMake
# targets, but we want to verify our pkg-config files remain usable and
# backwards compatible.
find_package(PkgConfig REQUIRED)

find_package(Threads REQUIRED)
# TODO(#5726) - DO NOT CHANGE THESE PACKAGE NAMES These package names are
# expected to remain usable until we have retired them.  This is a test for
# backwards compatibility. If you need to change the test, there is good chance
# you are breaking something.
find_package(bigtable_client REQUIRED)
find_package(firestore_client REQUIRED)
find_package(storage_client REQUIRED)
find_package(spanner_client REQUIRED)
find_package(pubsub_client REQUIRED)
pkg_check_modules(google_cloud_cpp_common IMPORTED_TARGET REQUIRED
                  google_cloud_cpp_common)
pkg_check_modules(google_cloud_cpp_grpc_utils IMPORTED_TARGET REQUIRED
                  google_cloud_cpp_grpc_utils)
pkg_check_modules(bigtable_client IMPORTED_TARGET REQUIRED bigtable_client)
pkg_check_modules(firestore_client IMPORTED_TARGET REQUIRED firestore_client)
pkg_check_modules(pubsub_client IMPORTED_TARGET REQUIRED pubsub_client)
pkg_check_modules(spanner_client IMPORTED_TARGET REQUIRED spanner_client)
pkg_check_modules(storage_client IMPORTED_TARGET REQUIRED storage_client)

include(CTest)

function (add_test_case name)
    add_executable("${name}" verify_exported_targets.cc)
    target_link_libraries("${name}" PRIVATE ${ARGN})
    add_test(NAME "${name}" COMMAND "${name}")
endfunction ()

# TODO(#5726) - DO NOT CHANGE THESE TARGET NAMES These package names are
# expected to remain usable until we have retired them.  This is a test for
# backwards compatibility. If you need to change the test, there is good chance
# you are breaking something.
add_test_case(t010 google_cloud_cpp_common)
add_test_case(t020 google_cloud_cpp_grpc_utils)
add_test_case(t030 bigtable_client)
add_test_case(t040 bigtable::client)
add_test_case(t050 bigtable_protos google_cloud_cpp_common)
add_test_case(t060 bigtable::protos google_cloud_cpp_common)
add_test_case(t070 firestore_client google_cloud_cpp_common)
add_test_case(t080 firestore::client google_cloud_cpp_common)
add_test_case(t090 googleapis-c++::pubsub_client)
add_test_case(t100 pubsub_client)
add_test_case(t120 storage_client)
add_test_case(t130 googleapis-c++::spanner_client)
add_test_case(t140 spanner_client)

add_test_case(t200 PkgConfig::google_cloud_cpp_common)
add_test_case(t210 PkgConfig::google_cloud_cpp_grpc_utils)
add_test_case(t220 PkgConfig::bigtable_client)
add_test_case(t230 PkgConfig::firestore_client
              PkgConfig::google_cloud_cpp_common)
add_test_case(t240 PkgConfig::pubsub_client)
add_test_case(t250 PkgConfig::spanner_client)
add_test_case(t260 PkgConfig::storage_client)
