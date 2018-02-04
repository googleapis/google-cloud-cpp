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
project(absl-installer CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(cctz)
add_subdirectory(abseil-cpp)

include(GNUInstallDirs)
install(DIRECTORY cctz/include/cctz
    DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY cctz/include/cctz
    DESTINATION include FILES_MATCHING PATTERN "*.inc")
install(DIRECTORY abseil-cpp/absl
    DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY abseil-cpp/absl
    DESTINATION include FILES_MATCHING PATTERN "*.inc")
install(FILES ${CMAKE_BINARY_DIR}/cctz/libcctz.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/algorithm/libabsl_algorithm.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/base/libabsl_base.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/container/libabsl_container.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/debugging/libabsl_debugging.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/memory/libabsl_memory.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/meta/libabsl_meta.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/numeric/libabsl_numeric.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/strings/libabsl_strings.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/synchronization/libabsl_synchronization.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/time/libabsl_time.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/types/libabsl_bad_any_cast.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/types/libabsl_optional.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/types/libabsl_bad_optional_access.a
    ${CMAKE_BINARY_DIR}/abseil-cpp/absl/utility/libabsl_utility.a
    DESTINATION ${CMAKE_INSTALL_LIBDIR})
