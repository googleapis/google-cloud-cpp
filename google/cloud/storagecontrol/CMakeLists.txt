# ~~~
# Copyright 2024 Google LLC
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

include(GoogleCloudCppLibrary)

google_cloud_cpp_add_gapic_library(storagecontrol "Cloud Storage API" TRANSITION
                                   SERVICE_DIRS "v2/")

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_library(
        storagecontrol_client_testing # cmake-format: sort
        v2/testing/remove_stale_folders.cc v2/testing/remove_stale_folders.h)
    target_link_libraries(storagecontrol_client_testing
                          PUBLIC google-cloud-cpp::storagecontrol)
    google_cloud_cpp_add_common_options(storagecontrol_client_testing)
    target_include_directories(
        storagecontrol_client_testing
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_compile_options(storagecontrol_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    create_bazel_config(storagecontrol_client_testing)

    # List the unit tests, then setup the targets and dependencies
    set(storagecontrol_client_unit_tests
        # cmake-format: sort
        v2/testing/remove_stale_folders_test.cc)
    foreach (fname ${storagecontrol_client_unit_tests})
        google_cloud_cpp_add_executable(target "storagecontrol" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE storagecontrol_client_testing google_cloud_cpp_testing
                    google-cloud-cpp::storagecontrol GTest::gmock_main
                    GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storagecontrol_client_unit_tests.bzl"
                         "storagecontrol_client_unit_tests" YEAR "2024")
    include(FindBenchmarkWithWorkarounds)

    add_executable(storagecontrol_quickstart "quickstart/quickstart.cc")
    target_link_libraries(storagecontrol_quickstart
                          PRIVATE google-cloud-cpp::storagecontrol)
    google_cloud_cpp_add_common_options(storagecontrol_quickstart)
    add_test(
        NAME storagecontrol_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:storagecontrol_quickstart>
            GOOGLE_CLOUD_CPP_STORAGE_TEST_FOLDER_BUCKET_NAME)
    set_tests_properties(storagecontrol_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()
