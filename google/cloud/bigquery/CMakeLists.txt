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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud BigQuery C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud BigQuery")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "bigquery_internal" "bigquery_testing"
                            "examples" "bigquery_v2_minimal_internal")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("bigquery" DEPENDS cloud-docs
                                 google-cloud-cpp::cloud_bigquery_protos)

include(GoogleCloudCppCommon)

if (bigquery IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    include(bigquery_grpc.cmake)
endif ()

if (experimental-bigquery_rest IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    include(bigquery_rest.cmake)
endif ()
