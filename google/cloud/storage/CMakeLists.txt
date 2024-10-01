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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/examples"
    "${CMAKE_CURRENT_SOURCE_DIR}/quickstart" "${CMAKE_CURRENT_SOURCE_DIR}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "storage_benchmarks" "storage_internal"
                            "examples")

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("storage" DEPENDS cloud-docs
                                 google-cloud-cpp::storage_protos)

include(GoogleCloudCppCommon)

include(IncludeNlohmannJson)
find_package(Crc32c)

# Export the version information for Bazel.
include(CreateBazelConfig)

# Enable unit tests
include(CTest)

include("${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_cpp_storage.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_cpp_storage_grpc.cmake")

# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (NOT GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    return()
endif ()

add_executable(storage_quickstart "quickstart/quickstart.cc")
target_link_libraries(storage_quickstart PRIVATE google-cloud-cpp::storage)
google_cloud_cpp_add_common_options(storage_quickstart)
add_test(
    NAME storage_quickstart
    COMMAND
        cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
        $<TARGET_FILE:storage_quickstart>
        GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME)
set_tests_properties(storage_quickstart
                     PROPERTIES LABELS "integration-test;quickstart")

if (NOT GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    return()
endif ()

add_executable(storage_quickstart_grpc "quickstart/quickstart_grpc.cc")
target_link_libraries(storage_quickstart_grpc
                      PRIVATE google-cloud-cpp::storage_grpc)
google_cloud_cpp_add_common_options(storage_quickstart_grpc)
add_test(
    NAME storage_quickstart_grpc
    COMMAND
        cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
        $<TARGET_FILE:storage_quickstart_grpc>
        GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME)
set_tests_properties(storage_quickstart_grpc
                     PROPERTIES LABELS "integration-test;quickstart")

add_executable(storage_quickstart_async "quickstart/quickstart_async.cc")
target_link_libraries(storage_quickstart_async
                      PRIVATE google-cloud-cpp::storage_grpc)
google_cloud_cpp_add_common_options(storage_quickstart_async)
add_test(
    NAME storage_quickstart_async
    COMMAND
        cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
        $<TARGET_FILE:storage_quickstart_async>
        GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME)
set_tests_properties(storage_quickstart_async
                     PROPERTIES LABELS "integration-test;quickstart")
