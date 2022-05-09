# ~~~
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# As long as this remains a header only library, macos idiosyncrasies require it
# to be an INTERFACE library.
add_library(google_cloud_cpp_testing_rest INTERFACE)
target_sources(
    google_cloud_cpp_testing_rest
    INTERFACE # cmake-format: sort
              ${CMAKE_CURRENT_SOURCE_DIR}/mock_http_payload.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mock_rest_client.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mock_rest_response.h)
target_link_libraries(google_cloud_cpp_testing_rest
                      INTERFACE google-cloud-cpp::common GTest::gmock)

create_bazel_config(google_cloud_cpp_testing_rest YEAR 2022)
