# ~~~
# Copyright 2019 Google LLC
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

find_package(Backtrace)

if ("${Backtrace_FOUND}")
    configure_file(backtrace.h.in backtrace.h)
    add_executable(object_write_deadlock_regression_test
                   object_write_deadlock_regression_test.cc)
    target_link_libraries(object_write_deadlock_regression_test
                          PRIVATE storage_client
                                  storage_client_testing
                                  google_cloud_cpp_testing
                                  google_cloud_cpp_common
                                  GTest::gmock_main
                                  GTest::gmock
                                  GTest::gtest
                                  CURL::libcurl
                                  Threads::Threads
                                  nlohmann_json
                                  storage_common_options)
    target_include_directories(object_write_deadlock_regression_test
                               PRIVATE "${Backtrace_INCLUDE_DIRS}")
    target_link_libraries(object_write_deadlock_regression_test
                          PRIVATE "${Backtrace_LIBRARIES}")
endif ()
