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

# abseil requires a gmock target to be defined.  Unlike the gtest target gRPC
# does not define it, so we must define it ourselves.  We do that in this file.

# Compile the googlemock library.  This library is rarely installed or
# pre-compiled because it should be configured with the same flags as the
# application.
add_library(gmock
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc
        ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/src/gmock-all.cc)
target_include_directories(gmock
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include
        PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock)

