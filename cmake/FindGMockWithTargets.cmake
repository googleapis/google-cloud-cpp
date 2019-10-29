# ~~~
# Copyright 2017 Google Inc.
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

# GTest always requires thread support.
find_package(Threads REQUIRED)

# When GTest is compiled with CMake, it exports GTest::gtest, GTest::gmock,
# GTest::gtest_main and GTest::gmock_main as link targets. On the other hand,
# the standard CMake module to discover GTest, it exports GTest::GTest, and does
# not export GTest::gmock.
#
# In this file we try to normalize the situation to the packages defined in the
# source.  Not perfect, but better than the mess we have otherwise.

function (google_cloud_cpp_create_googletest_aliases)
    # FindGTest() is a standard CMake module. It, unfortunately, *only* creates
    # targets for the googletest libraries (not gmock), and with a name that is
    # not the same names used by googletest: GTest::GTest vs. GTest::gtest and
    # GTest::Main vs. GTest::gtest_main. We create aliases for them:
    add_library(GTest_gtest INTERFACE)
    target_link_libraries(GTest_gtest INTERFACE GTest::GTest)
    add_library(GTest_gtest_main INTERFACE)
    target_link_libraries(GTest_gtest_main INTERFACE GTest::Main)
    add_library(GTest::gtest ALIAS GTest_gtest)
    add_library(GTest::gtest_main ALIAS GTest_gtest_main)
endfunction ()

function (google_cloud_cpp_gmock_library_import_location target lib)
    find_library(_library_release ${lib})
    find_library(_library_debug ${lib}d)

    if ("${_library_debug}" MATCHES "-NOTFOUND" AND "${_library_release}"
                                                    MATCHES "-NOTFOUND")
        message(FATAL_ERROR "Cannot find library ${lib} for ${target}.")
    elseif ("${_library_debug}" MATCHES "-NOTFOUND")
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION
                                                   "${_library_release}")
    elseif ("${_library_release}" MATCHES "-NOTFOUND")
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION
                                                   "${_library_debug}")
    else ()
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION_DEBUG
                                                   "${_library_debug}")
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION_RELEASE
                                                   "${_library_release}")
    endif ()
endfunction ()

function (google_cloud_cpp_transfer_library_properties target source)

    add_library(${target} UNKNOWN IMPORTED)
    get_target_property(value ${source} IMPORTED_LOCATION)
    if (NOT value)
        get_target_property(value ${source} IMPORTED_LOCATION_DEBUG)
        if (EXISTS "${value}")
            set_target_properties(${target} PROPERTIES IMPORTED_LOCATION
                                                       ${value})
        endif ()
        get_target_property(value ${source} IMPORTED_LOCATION_RELEASE)
        if (EXISTS "${value}")
            set_target_properties(${target} PROPERTIES IMPORTED_LOCATION
                                                       ${value})
        endif ()
    endif ()
    foreach (
        property
        IMPORTED_LOCATION_DEBUG
        IMPORTED_LOCATION_RELEASE
        IMPORTED_CONFIGURATIONS
        INTERFACE_INCLUDE_DIRECTORIES
        IMPORTED_LINK_INTERFACE_LIBRARIES
        IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG
        IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE)
        get_target_property(value ${source} ${property})
        message("*** ${source} ${property} ${value}")
        if (value)
            set_target_properties(${target} PROPERTIES ${property} "${value}")
        endif ()
    endforeach ()
endfunction ()

include(CTest)
if (TARGET GTest::gmock)
    # GTest::gmock is already defined, do not define it again.
elseif (NOT BUILD_TESTING AND NOT GOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL)
    # Tests are turned off via -DBUILD_TESTING, do not load the googletest or
    # googlemock dependency.
else ()
    # Try to find the config package first. If that is not found
    find_package(GTest CONFIG QUIET)
    find_package(GMock CONFIG QUIET)
    if (NOT GTest_FOUND)
        find_package(GTest MODULE REQUIRED)

        google_cloud_cpp_create_googletest_aliases()

        # The FindGTest module finds GTest by default, but does not search for
        # GMock, though they are usually installed together. Define the
        # GTest::gmock* targets manually.
        find_path(
            GMOCK_INCLUDE_DIR gmock/gmock.h
            HINTS $ENV{GTEST_ROOT}/include ${GTEST_ROOT}/include
            DOC "The GoogleTest Mocking Library headers")
        if ("${GMOCK_INCLUDE_DIR}" MATCHES "-NOTFOUND")
            message(
                FATAL_ERROR "Cannot find gmock headers ${GMOCK_INCLUDE_DIR}.")
        endif ()
        mark_as_advanced(GMOCK_INCLUDE_DIR)

        add_library(GTest::gmock UNKNOWN IMPORTED)
        google_cloud_cpp_gmock_library_import_location(GTest::gmock gmock)
        set_target_properties(
            GTest::gmock
            PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                       "GTest::GTest;Threads::Threads"
                       INTERFACE_INCLUDE_DIRECTORIES "${GMOCK_INCLUDE_DIRS}")

        add_library(GTest::gmock_main UNKNOWN IMPORTED)
        google_cloud_cpp_gmock_library_import_location(GTest::gmock_main
                                                       gmock_main)
        set_target_properties(
            GTest::gmock_main
            PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                       "GTest::gmock;Threads::Threads"
                       INTERFACE_INCLUDE_DIRECTORIES "${GMOCK_INCLUDE_DIRS}")
    endif ()

    if (NOT TARGET GTest::gmock AND TARGET GMock::gmock)
        google_cloud_cpp_transfer_library_properties(GTest::gmock GMock::gmock)
        google_cloud_cpp_transfer_library_properties(GTest::gmock_main
                                                     GMock::gmock_main)
    endif ()
endif ()
