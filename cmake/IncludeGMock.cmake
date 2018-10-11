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

set(GOOGLE_CLOUD_CPP_GMOCK_PROVIDER "external"
    CACHE STRING "How to find the googlemock library")
set_property(CACHE GOOGLE_CLOUD_CPP_GMOCK_PROVIDER
             PROPERTY STRINGS
                      "external"
                      "package"
                      "vcpkg"
                      "pkg-config")

if (TARGET gmock)
    # gmock is already defined, do not define it again.
elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "external")
    include(external/googletest)

    # On Windows GTest uses library postfixes for debug versions, that is
    # gtest.lib becomes gtestd.lib when compiled with for debugging.  This ugly
    # expression computes that value. Note that it must be a generator
    # expression because with MSBuild the config type can change after the
    # configuration phase.
    if (WIN32)
        set(_lib_postfix $<$<CONFIG:DEBUG>:d>)
    endif ()

    include(ExternalProjectHelper)
    add_library(GTest::gtest INTERFACE IMPORTED)
    add_dependencies(GTest::gtest googletest_project)
    set_library_properties_for_external_project(GTest::gtest
                                                gtest${_lib_postfix})
    set_property(TARGET GTest::gtest
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES "Threads::Threads")

    add_library(GTest::gtest_main INTERFACE IMPORTED)
    add_dependencies(GTest::gtest_main googletest_project)
    set_library_properties_for_external_project(GTest::gtest_main
                                                gtest_main${_lib_postfix})
    set_property(TARGET GTest::gtest_main
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          "GTest::gtest;Threads::Threads")

    add_library(GTest::gmock INTERFACE IMPORTED)
    add_dependencies(GTest::gmock googletest_project)
    set_library_properties_for_external_project(GTest::gmock
                                                gmock${_lib_postfix})
    set_property(TARGET GTest::gmock
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          "GTest::gtest;Threads::Threads")

    add_library(GTest::gmock_main INTERFACE IMPORTED)
    add_dependencies(GTest::gmock_main googletest_project)
    set_library_properties_for_external_project(GTest::gmock_main
                                                gmock_main${_lib_postfix})
    set_property(TARGET GTest::gmock_main
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          "GTest::gmock;GTest::gtest;Threads::Threads")

    # TODO(#310) - consider changing the name of this target as it can conflict
    # with another target defined in a submodule.
    add_library(gmock INTERFACE)
    target_link_libraries(gmock
                          INTERFACE GTest::gmock_main GTest::gmock GTest::gtest)

elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "vcpkg")
    find_package(GTest REQUIRED)

    # The FindGTest module finds GTest by default, but does not search for
    # GMock, though they are usually installed together. Define the
    # GTest::gmock* targets manually.
    __gtest_find_library(GMOCK_LIBRARY gmock)
    __gtest_find_library(GMOCK_LIBRARY_DEBUG gmockd)
    if ("${GMOCK_LIBRARY}" MATCHES "-NOTFOUND")
        message(FATAL_ERROR "Cannot find gmock library ${GMOCK_LIBRARY}.")
    endif ()
    mark_as_advanced(GMOCK_LIBRARY)

    find_path(GMOCK_INCLUDE_DIR gmock/gmock.h
              HINTS $ENV{GTEST_ROOT}/include ${GTEST_ROOT}/include
              DOC "The GoogleTest Mocking Library headers")
    if ("${GMOCK_INCLUDE_DIR}" MATCHES "-NOTFOUND")
        message(FATAL_ERROR "Cannot find gmock library ${GMOCK_INCLUDE_DIR}.")
    endif ()
    mark_as_advanced(GMOCK_INCLUDE_DIR)

    add_library(GTest::gmock STATIC IMPORTED)
    set_target_properties(GTest::gmock
                          PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                                     GTest::GTest
                                     INTERFACE_INCLUDE_DIRECTORIES
                                     "${GMOCK_INCLUDE_DIRS}")
    __gtest_import_library(GTest::gmock GMOCK_LIBRARY "")
    __gtest_import_library(GTest::gmock GMOCK_LIBRARY "RELEASE")
    __gtest_import_library(GTest::gmock GMOCK_LIBRARY "DEBUG")

    # TODO(#310) - consider changing the name of this target as it can conflict
    # with another target defined in a submodule.
    add_library(gmock INTERFACE)
    target_link_libraries(gmock INTERFACE GTest::gmock)

elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "package")
    find_package(Threads REQUIRED)
    find_package(GTest REQUIRED)

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

    find_library(GMOCK_LIBRARY gmock)
    if ("${GMOCK_LIBRARY}" MATCHES "-NOTFOUND")
        message(FATAL_ERROR "Cannot find gmock library ${GMOCK_LIBRARY}.")
    endif ()
    mark_as_advanced(GMOCK_LIBRARY)

    find_library(GMOCK_MAIN_LIBRARY gmock_main)
    if ("${GMOCK_LIBRARY}" MATCHES "-NOTFOUND")
        message(
            FATAL_ERROR "Cannot find gmock_main library ${GMOCK_MAIN_LIBRARY}.")
    endif ()
    mark_as_advanced(GMOCK_MAIN_LIBRARY)

    add_library(GTest::gmock UNKNOWN IMPORTED)
    set_target_properties(GTest::gmock
                          PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                                     "GTest::GTest;Threads::Threads"
                                     INTERFACE_INCLUDE_DIRECTORIES
                                     "${GMOCK_INCLUDE_DIRS}"
                                     IMPORTED_LOCATION
                                     "${GMOCK_LIBRARY}")

    add_library(GTest::gmock_main UNKNOWN IMPORTED)
    set_target_properties(GTest::gmock_main
                          PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES
                                     "GTest::gmock;Threads::Threads"
                                     INTERFACE_INCLUDE_DIRECTORIES
                                     "${GMOCK_INCLUDE_DIRS}"
                                     IMPORTED_LOCATION
                                     "${GMOCK_MAIN_LIBRARY}")

    # TODO(#310) - consider changing the name of this target as it can conflict
    # with another target defined in a submodule.
    add_library(gmock INTERFACE)
    target_link_libraries(gmock
                          INTERFACE GTest::gmock_main GTest::gmock GTest::GTest)

elseif("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "pkg-config")

    # Use pkg-config to find the libraries.
    find_package(PkgConfig REQUIRED) # We need a helper function to convert pkg-
                                     # config(1) output into target  properties.
    include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake)

    pkg_check_modules(gmock_pc REQUIRED gmock_main gmock gtest)
    add_library(GTest::gmock_main INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(GTest::gmock_main gmock_pc)
    set_property(TARGET GTest::gmock_main
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)

    # TODO(#310) - consider changing the name of this target as it can conflict
    # with another target defined in a submodule.
    add_library(gmock INTERFACE)
    target_link_libraries(gmock INTERFACE GTest::gmock_main)
endif ()
