# ~~~
# Copyright 2021 Google LLC
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

file(
    GLOB v2_source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "v2/minimal/internal/*.h" "v2/minimal/internal/*.cc")
list(SORT v2_source_files)
add_library(bigquery_v2_minimal_internal ${v2_source_files})
target_include_directories(
    bigquery_v2_minimal_internal
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    bigquery_v2_minimal_internal PUBLIC google-cloud-cpp::rest_internal
                                        google-cloud-cpp::common)
google_cloud_cpp_add_common_options(bigquery_v2_minimal_internal)
set_target_properties(
    bigquery_v2_minimal_internal
    PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery_v2_minimal
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(bigquery_v2_minimal_internal
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::bigquery_v2_minimal ALIAS
            bigquery_v2_minimal_internal)
