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
find_package(google_cloud_cpp_common REQUIRED)
find_package(google_cloud_cpp_grpc_utils REQUIRED)
find_package(google_cloud_cpp_bigtable REQUIRED)
find_package(google_cloud_cpp_firestore REQUIRED)
find_package(google_cloud_cpp_pubsub REQUIRED)
find_package(spanner_client REQUIRED)
find_package(storage_client REQUIRED)

include(CTest)

function (add_test_case name)
    add_executable("${name}" verify_current_targets.cc)
    target_link_libraries("${name}" PRIVATE ${ARGN})
    add_test(NAME "${name}" COMMAND "${name}")
endfunction ()

add_test_case(t010 google-cloud-cpp::common)
add_test_case(t020 google-cloud-cpp::grpc_utils)
add_test_case(t030 google-cloud-cpp::bigtable)
add_test_case(t040 google-cloud-cpp::experimental-firestore)
add_test_case(t100 google-cloud-cpp::pubsub)
add_test_case(t110 spanner_client)
add_test_case(t120 storage_client)

set(pkg_config_modules
    # cmake-format: sortable
    google_cloud_cpp_common google_cloud_cpp_grpc_utils
    google_cloud_cpp_bigtable google_cloud_cpp_pubsub spanner_client storage_client)
foreach (module ${pkg_config_modules})
    pkg_check_modules(${module} IMPORTED_TARGET REQUIRED ${module})
    add_test_case(test_pc_${module} PkgConfig::${module})
endforeach ()
