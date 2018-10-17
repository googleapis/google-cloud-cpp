# ~~~
# Copyright 2018 Google LLC
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

set(storage_client_integration_tests
    bucket_integration_test.cc
    curl_upload_request_integration_test.cc
    curl_download_request_integration_test.cc
    curl_request_integration_test.cc
    curl_streambuf_integration_test.cc
    object_media_integration_test.cc
    object_integration_test.cc
    service_account_integration_test.cc
    storage_include_test.cc
    thread_integration_test.cc)

foreach (fname ${storage_client_integration_tests})
    string(REPLACE "/"
                   "_"
                   target
                   ${fname})
    string(REPLACE ".cc"
                   ""
                   target
                   ${target})
    add_executable(${target} ${fname})
    target_link_libraries(${target}
                          PRIVATE storage_client
                                  storage_client_testing
                                  google_cloud_cpp_testing
                                  google_cloud_cpp_common
                                  GTest::gmock_main
                                  GTest::gmock
                                  GTest::gtest
                                  CURL::CURL
                                  Threads::Threads
                                  nlohmann_json
                                  storage_common_options)
endforeach ()

include(CreateBazelConfig)
export_list_to_bazel("storage_client_integration_tests.bzl"
                     "storage_client_integration_tests")
