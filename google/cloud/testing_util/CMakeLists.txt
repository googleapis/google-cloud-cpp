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

# We check this separately. A library may wish to use the testing util lib but
# not build all of the tests.
if (BUILD_TESTING OR GOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL)
    add_library(google_cloud_cpp_testing
                assert_ok.cc
                assert_ok.h
                capture_log_lines_backend.h
                capture_log_lines_backend.cc
                check_predicate_becomes_false.h
                chrono_literals.h
                environment_variable_restore.h
                environment_variable_restore.cc
                expect_exception.h
                expect_future_error.h
                custom_google_mock_main.cc
                init_google_mock.h
                init_google_mock.cc
                testing_types.h
                testing_types.cc)
    target_link_libraries(google_cloud_cpp_testing
                          PUBLIC google_cloud_cpp_common GTest::gmock
                          PRIVATE google_cloud_cpp_common_options)

    create_bazel_config(google_cloud_cpp_testing YEAR 2019)

    set(google_cloud_cpp_testing_unit_tests assert_ok_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_testing_unit_tests.bzl"
                         "google_cloud_cpp_testing_unit_tests")

    foreach (fname ${google_cloud_cpp_testing_unit_tests})
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
                              PRIVATE google_cloud_cpp_testing
                                      google_cloud_cpp_common
                                      GTest::gmock_main
                                      GTest::gmock
                                      GTest::gtest
                                      google_cloud_cpp_common_options)
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()

    # Export the CMake targets to make it easy to create configuration files.
    install(
        EXPORT google_cloud_cpp_testing-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_testing")

    # Install the libraries and headers in the locations determined by
    # GNUInstallDirs
    install(TARGETS google_cloud_cpp_testing
            EXPORT google_cloud_cpp_testing-targets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    google_cloud_cpp_install_headers(google_cloud_cpp_testing
                                     include/google/cloud/testing_util)

    # Setup global variables used in the following *.in files.
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${GOOGLE_CLOUD_CPP_VERSION_MINOR})
    set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${GOOGLE_CLOUD_CPP_VERSION_PATCH})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "Google Cloud C++ Client Library Testing Utilities")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
        "Testing Utilities used by the Google Cloud C++ Client Libraries.")
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_testing")

    # Create and install the pkg-config files.
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
                   "google_cloud_cpp_testing.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_testing.pc"
                  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Create and install the CMake configuration files.
    configure_file("config.cmake.in" "google_cloud_cpp_testing-config.cmake"
                   @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
                   "google_cloud_cpp_testing-config-version.cmake" @ONLY)
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_testing-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_testing-config-version.cmake"
            "${PROJECT_SOURCE_DIR}/cmake/FindGMockWithTargets.cmake"
            DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_testing")
endif ()
