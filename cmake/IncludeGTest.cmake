# Copyright 2017 Google Inc.
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

# abseil requires a gtest target to be defined.  When gRPC is used as a module,
# the target is defined (indirectly) by boringssl, and we cannot define it
# ourselves.  When compiling with a installed gRPC gtest is not defined, and
# we must define it ourselves.  We do that in this file.

# Compile the gtest library.  This library is rarely installed or
# pre-compiled because it should be configured with the same flags as the
# application.
add_library(gtest
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc)
target_include_directories(gtest
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest)

# Compile the gtest_main library.  This library is rarely installed or
# pre-compiled because it should be configured with the same flags as the
# application.
add_library(gtest_main
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc)
target_include_directories(gtest_main
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest)
add_dependencies(gtest_main gtest)
