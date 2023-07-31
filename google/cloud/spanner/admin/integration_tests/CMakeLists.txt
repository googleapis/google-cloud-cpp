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

function (spanner_admin_define_integration_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(spanner_admin_integration_tests
        # cmake-format: sort
        backup_extra_integration_test.cc backup_integration_test.cc
        database_admin_integration_test.cc instance_admin_integration_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_admin_integration_tests.bzl"
                         "spanner_admin_integration_tests" YEAR "2021")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(spanner-admin-integration-tests)

    # Generate a target for each unit test.
    foreach (fname ${spanner_admin_integration_tests})
        google_cloud_cpp_add_executable(target "spanner_admin" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::spanner spanner_client_testing
                    google_cloud_cpp_testing_grpc GTest::gmock_main
                    GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target} PROPERTIES LABELS
                                 "integration-test;integration-test-emulator")
        add_dependencies(spanner-admin-integration-tests ${target})
    endforeach ()
    if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        return()
    endif ()
endfunction ()

function (spanner_admin_define_integration_tests_rest)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(spanner_admin_integration_tests_rest
        # cmake-format: sort
        instance_admin_rest_integration_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_admin_integration_tests_rest.bzl"
                         "spanner_admin_integration_tests_rest" YEAR "2023")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(spanner-admin-integration-tests-rest)

    # Generate a target for each unit test.
    foreach (fname ${spanner_admin_integration_tests_rest})
        google_cloud_cpp_add_executable(target "spanner_admin_rest" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::spanner_rest spanner_client_testing
                    google_cloud_cpp_testing_grpc GTest::gmock_main
                    GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target}
            PROPERTIES
                LABELS
                "integration-test;integration-test-emulator;http-transcoding")
        add_dependencies(spanner-admin-integration-tests-rest ${target})
    endforeach ()
    if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        return()
    endif ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (NOT BUILD_TESTING)
    return()
endif ()

spanner_admin_define_integration_tests()
if ("experimental-http-transcoding" IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    spanner_admin_define_integration_tests_rest()
endif ()
