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

# Configure the googletest dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_GMOCK_PROVIDER "module"
        CACHE STRING "How to find the googlemock library")
set_property(CACHE GOOGLE_CLOUD_CPP_GMOCK_PROVIDER
        PROPERTY STRINGS "module" "package" "vcpkg" "pkg-config")

if ("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "module")
    # Compile the googlemock library.  This library is rarely installed or
    # pre-compiled because it should be configured with the same flags as the
    # application.
    add_library(gmock
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc
            ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/src/gmock-all.cc)
    target_include_directories(gmock
            PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
            PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest
            PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include
            PUBLIC ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock)

elseif ("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "vcpkg")
    find_package(GTest REQUIRED)
    # The FindGTest module can find GTest when installed with vcpkg, but does not find GMock.
    __gtest_find_library(GMOCK_LIBRARY            gmock)
    __gtest_find_library(GMOCK_LIBRARY_DEBUG      gmockd)
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

    add_library(GMock::GMock STATIC IMPORTED)
    set_target_properties(GMock::GMock PROPERTIES
            IMPORTED_LINK_INTERFACE_LIBRARIES GTest::GTest
            INTERFACE_INCLUDE_DIRECTORIES "${GMOCK_INCLUDE_DIRS}")
    __gtest_import_library(GMock::GMock GMOCK_LIBRARY "")
    __gtest_import_library(GMock::GMock GMOCK_LIBRARY "RELEASE")
    __gtest_import_library(GMock::GMock GMOCK_LIBRARY "DEBUG")
    add_library(gmock INTERFACE)
    target_link_libraries(gmock INTERFACE GMock::GMock GTest::Main)

elseif ("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "package")
    message(FATAL_ERROR "GOOGLE_CLOUD_CPP_GMOCK_PROVIDER=package is not implemented.")
elseif ("${GOOGLE_CLOUD_CPP_GMOCK_PROVIDER}" STREQUAL "pkg-config")
    message(FATAL_ERROR "GOOGLE_CLOUD_CPP_GMOCK_PROVIDER=pkg-config is not implemented.")
endif ()
