# ~~~
# Copyright 2018 Google LLC
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

set(DOXYGEN_PROJECT_NAME "Google Cloud C++ Client")
set(DOXYGEN_PROJECT_BRIEF "C++ Client Library for Google Cloud Platform")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "testing_util" "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::rpc_status_protos
                                  google-cloud-cpp::bigtable_protos)

include(GoogleCloudCppCommon)
include(CreateBazelConfig)
include(CMakePackageConfigHelpers)

# Define a CMake configuration option to set the build metadata. By default this
# is not initialized.
set(GOOGLE_CLOUD_CPP_BUILD_METADATA
    ""
    CACHE STRING "Append build metadata to the library version number")
# This option is rarely needed. Mark it as "advanced" to remove it from the
# default CMake UIs.
mark_as_advanced(GOOGLE_CLOUD_CPP_BUILD_METADATA)

include("${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_cpp_common.cmake")

add_subdirectory(testing_util)

if (GOOGLE_CLOUD_CPP_ENABLE_GRPC)
    include("${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_cpp_grpc_utils.cmake")
endif ()

if (GOOGLE_CLOUD_CPP_ENABLE_REST)
    include("${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_cpp_rest_internal.cmake")
endif ()
