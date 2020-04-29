# ~~~
# Copyright 2020 Google LLC
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
function (google_cloud_cpp_test_name_to_target var fname)
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set("${var}"
        "${target}"
        PARENT_SCOPE)
endfunction ()

function (google_cloud_cpp_pubsub_define_integration_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    set(pubsub_client_integration_tests
        # cmake-format: sortable
        publisher_admin_integration_test.cc
        subscriber_admin_integration_test.cc)
    set(pubsub_client_install_tests pubsub_install_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("pubsub_client_integration_tests.bzl"
                         "pubsub_client_integration_tests" YEAR "2020")
    export_list_to_bazel("pubsub_client_install_tests.bzl"
                         "pubsub_client_install_tests" YEAR "2020")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(pubsub-client-integration-tests)

    # Generate a target for each unit test.
    foreach (fname ${pubsub_client_integration_tests})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        add_executable(${target} ${fname})
        target_link_libraries(
            ${target}
            PRIVATE googleapis-c++::pubsub_client google_cloud_cpp_testing
                    GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        google_cloud_cpp_add_clang_tidy(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target}
            PROPERTIES LABELS "integration-tests;integration-tests-no-emulator")
        add_dependencies(pubsub-client-integration-tests ${target})
    endforeach ()

    if (MSVC)
        # MSVC warns about using localtime(), this is a valuable warning, so we
        # do not want to disable for everything. But for these files the usage
        # is "safe". This is a relatively obscure feature to add compile flags
        # to just one source:
        set_property(
            SOURCE instance_admin_integration_test.cc
            APPEND_STRING
            PROPERTY COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
    endif ()

    if (NOT GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        return()
    endif ()

    foreach (fname ${pubsub_client_install_tests})
        google_cloud_cpp_test_name_to_target(target "${fname}")
        add_executable(${target} ${fname})
        target_link_libraries(${target} PRIVATE googleapis-c++::pubsub_client)
        google_cloud_cpp_add_common_options(${target})
        google_cloud_cpp_add_clang_tidy(${target})

        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(
            ${target}
            PROPERTIES LABELS "integration-tests;integration-tests-no-emulator")
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_pubsub_define_integration_tests()
endif (BUILD_TESTING)
