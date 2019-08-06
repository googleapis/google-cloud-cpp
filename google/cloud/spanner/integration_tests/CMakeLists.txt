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

include(EnableClangTidy)
include(EnableWerror)

function (spanner_client_define_integration_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    find_package(google_cloud_cpp_testing CONFIG REQUIRED)

    set(spanner_client_integration_tests database_admin_integration_test.cc
                                         mutate_and_read_integration_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("spanner_client_integration_tests.bzl"
                         "spanner_client_integration_tests")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(spanner-client-integration-tests)

    # Generate a target for each unit test.
    foreach (fname ${spanner_client_integration_tests})
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
                              PRIVATE googleapis-c++::spanner_client
                                      spanner_client_testing
                                      google_cloud_cpp_testing
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest)
        google_cloud_cpp_add_clang_tidy(${target})
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS "integration-tests")
        add_dependencies(spanner-client-integration-tests ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    spanner_client_define_integration_tests()
endif (BUILD_TESTING)
