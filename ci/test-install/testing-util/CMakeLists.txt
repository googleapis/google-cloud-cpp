# Copyright 2019 Google LLC
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

# A minimal CMakeList.txt to *test* the install of the testing utility header
# files.
#
# This is not intended to be a demonstration of how to write good CMakeList.txt
# files, nor is it a general solution to create CMake files for google-cloud-
# cpp. It is simply a minimal CMake file to verify that (a) the installed
# libraries contain all required headers, and (b) the installed CMake
# configuration files are usable.

cmake_minimum_required(VERSION 3.5)
project(google-cloud-cpp-testing-util-install-integration-test CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# testing_util depends upon google_cloud_cpp_common. It is currently not a
# separate library.
find_package(google_cloud_cpp_common REQUIRED)

add_executable(testing_util_install_test testing_util_install_test.cc)
target_link_libraries(testing_util_install_test GTest::gmock
                      google_cloud_cpp_common)
