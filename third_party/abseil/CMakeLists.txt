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

cmake_minimum_required(VERSION 3.5)
project(absl)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/abseil-cpp/CMake)

include(GNUInstallDirs)
include(CMakeParseArguments)
include(abseil-cpp/CMake/AbseilHelpers.cmake)

# Some common definitions shared by all the targets.
list(APPEND ABSL_COMMON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/abseil-cpp)

# Only check for gtest and friends if we are building the tests.
if (BUILD_TESTING)
    check_target(gtest)
    check_target(gtest_main)
    check_target(gmock)
    list(APPEND ABSL_TEST_COMMON_LIBRARIES
        gtest_main
        gtest
        gmock
        ${CMAKE_THREAD_LIBS_INIT}
        )
endif (BUILD_TESTING)

# -fexceptions
set(ABSL_EXCEPTIONS_FLAG "${CMAKE_CXX_EXCEPTIONS}")

# fix stuff
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FIX_MSVC} ${CMAKE_CXX_FLAGS}")

# Override the definitions of key absl_* functions.

# Call the original function (with its name prefixed by `_`), and add the
# install targets for it.
function(absl_header_library)
    _absl_header_library(${ARGN})
    cmake_parse_arguments(ABSL_HO_LIB
        "DISABLE_INSTALL"
        "EXPORT_NAME;TARGET"
        "PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS"
        ${ARGN}
        )
    string(REPLACE "absl_" "" _SHORT_NAME ${ABSL_HO_LIB_TARGET})
    install(CODE "message(\"Installing ${ABSL_HO_LIB_TARGET} headers.\")")
    install(DIRECTORY . DESTINATION include/absl/${_SHORT_NAME} FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY . DESTINATION include/absl/${_SHORT_NAME} FILES_MATCHING PATTERN "*.inc")
endfunction(absl_header_library)

# This function is a copy completely copied, because we need to replace the add_library() line.
function(absl_library)
    cmake_parse_arguments(ABSL_LIB
            "DISABLE_INSTALL" # keep that in case we want to support installation one day
            "TARGET;EXPORT_NAME"
            "SOURCES;PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS"
            ${ARGN}
            )

    set(_NAME ${ABSL_LIB_TARGET})
    string(TOUPPER ${_NAME} _UPPER_NAME)

    add_library(${_NAME} ${ABSL_LIB_SOURCES})

    target_compile_options(${_NAME} PRIVATE ${ABSL_COMPILE_CXXFLAGS} ${ABSL_LIB_PRIVATE_COMPILE_FLAGS})
    target_link_libraries(${_NAME} PUBLIC ${ABSL_LIB_PUBLIC_LIBRARIES})
    target_include_directories(${_NAME}
            PUBLIC ${ABSL_COMMON_INCLUDE_DIRS} ${ABSL_LIB_PUBLIC_INCLUDE_DIRS}
            PRIVATE ${ABSL_LIB_PRIVATE_INCLUDE_DIRS}
            )

    if (ABSL_LIB_EXPORT_NAME)
        add_library(absl::${ABSL_LIB_EXPORT_NAME} ALIAS ${_NAME})
    endif ()
    string(REPLACE "absl_" "" _SHORT_NAME ${ABSL_LIB_TARGET})
    install(CODE "message(\"Installing ${ABSL_LIB_TARGET} libraries and headers.\")")
    install(TARGETS ${_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY . DESTINATION include/absl/${_SHORT_NAME} FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY . DESTINATION include/absl/${_SHORT_NAME} FILES_MATCHING PATTERN "*.inc")
endfunction()

find_package(Threads REQUIRED)

check_target(cctz)

add_subdirectory(abseil-cpp/absl third_party/abseil)
