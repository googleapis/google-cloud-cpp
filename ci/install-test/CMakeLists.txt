# Copyright 2018 Google Inc.
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

cmake_minimum_required(VERSION 3.5)
project(google-cloud-cpp-install-integration-test CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT TARGET bigtable::client)
    # To avoid bitrot this test is compiled as part of the regular build. In
    # that case bigtable::client is a target and there is no need to import
    # any other targets, but when used for testing installs then the following
    # becomes necessary.
    find_package(bigtable_client REQUIRED)
    find_package(bigtable_admin_client REQUIRED)
endif ()

# Defining new targets should be easy.
add_executable(install_integration_test install_integration_test.cc)
target_link_libraries(install_integration_test bigtable::client bigtable::admin_client)
