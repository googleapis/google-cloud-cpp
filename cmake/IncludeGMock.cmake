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

# Configure the googletest dependency, this can be found as an external project,
# a package, or installed with pkg-config support. When GTest compiles itself,
# it exports GTest::gtest, GTest::gmock, GTest::gtest_main and GTest::gmock_main
# as the link targets. The standard CMake module to discover GTest, however,
# exports GTest::GTest, and does not export GTest::gmock.
#
# In this file we try to normalize the situation to the packages defined in the
# source.  Not perfect, but better than the mess we have otherwise.

set(GOOGLE_CLOUD_CPP_GMOCK_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
    CACHE STRING "How to find the googlemock library")
set_property(CACHE GOOGLE_CLOUD_CPP_GMOCK_PROVIDER
             PROPERTY STRINGS "external" "package")

function (create_googletest_aliases)
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

include(CTest)
if (TARGET GTest::gmock)
    # GTest::gmock is already defined, do not define it again.
elseif(NOT BUILD_TESTING)
    # Tests are turned off via -DBUILD_TESTING, do not load the googletest or
    # googlemock dependency.
elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "external")
    include(external/googletest)
elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "package")
    find_package(GTest REQUIRED)

    create_googletest_aliases()

    # The FindGTest module finds GTest by default, but does not search for
    # GMock, though they are usually installed together. Define the
    # GTest::gmock* targets manually.
    find_path(GMOCK_INCLUDE_DIR gmock/gmock.h
              HINTS $ENV{GTEST_ROOT}/include ${GTEST_ROOT}/include
              DOC "The GoogleTest Mocking Library headers")
    if ("${GMOCK_INCLUDE_DIR}" MATCHES "-NOTFOUND")
        message(FATAL_ERROR "Cannot find gmock headers ${GMOCK_INCLUDE_DIR}.")
    endif ()
    mark_as_advanced(GMOCK_INCLUDE_DIR)

    function (__gmock_library_import_location target lib)
        find_library(_library_release ${lib})
        find_library(_library_debug ${lib}d)

        if ("${_library_debug}" MATCHES "-NOTFOUND"
            AND "${_library_release}" MATCHES "-NOTFOUND")
            message(FATAL_ERROR "Cannot find library ${lib} for ${target}.")
        elseif("${_library_debug}" MATCHES "-NOTFOUND")
            set_target_properties(${target}
                                  PROPERTIES IMPORTED_LOCATION
                                             "${_library_release}")
        elseif("${_library_release}" MATCHES "-NOTFOUND")
            set_target_properties(${target}
                                  PROPERTIES IMPORTED_LOCATION
                                             "${_library_debug}")
        else()
            set_target_properties(${target}
                                  PROPERTIES IMPORTED_LOCATION_DEBUG
                                             "${_library_debug}")
            set_target_properties(${target}
                                  PROPERTIES IMPORTED_LOCATION_RELEASE
                                             "${_library_release}")
        endif ()
    endfunction ()

    add_library(GTest::gmock UNKNOWN IMPORTED)
    __gmock_library_import_location(GTest::gmock gmock)
    set_target_properties(GTest::gmock
                          PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                                     "GTest::GTest;Threads::Threads"
                                     INTERFACE_INCLUDE_DIRECTORIES
                                     "${GMOCK_INCLUDE_DIRS}")

    add_library(GTest::gmock_main UNKNOWN IMPORTED)
    __gmock_library_import_location(GTest::gmock_main gmock_main)
    set_target_properties(GTest::gmock_main
                          PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                                     "GTest::gmock;Threads::Threads"
                                     INTERFACE_INCLUDE_DIRECTORIES
                                     "${GMOCK_INCLUDE_DIRS}")
endif ()
