# Copyright 2018 Google LLC
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

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# the client library
add_library(firestore_client
    field_path.h
    field_path.cc)
target_link_libraries(firestore_client)
target_include_directories(firestore_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(firestore_client PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(firestore::client ALIAS firestore_client)

option(FIRESTORE_CLIENT_CLANG_TIDY
    "If set compiles the Cloud Firestore client with clang-tidy."
    "")
if (CLANG_TIDY_EXE AND FIRESTORE_CLIENT_CLANG_TIDY)
    message(STATUS "clang-tidy build enabled.")
    set_target_properties(
        firestore_client PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif ()

add_library(firestore_client_testing
        field_path.h
	field_path_test.cc)

# List the unit tests, then setup the targets and dependencies.
set(firestore_client_unit_tests
  	field_path_test.cc)

foreach (fname ${firestore_client_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "firestore_client_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
            firestore_client_testing firestore_client gtest pthread)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

install(TARGETS firestore_client
    EXPORT firestore-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY . DESTINATION include/firestore/client
        FILES_MATCHING PATTERN "*.h"
        PATTERN "testing/*" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
    DESTINATION include/firestore/client)

# Install the pkg-config files.
configure_file("config.pc.in" "firestore_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/firestore_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Install the CMake configuration files.
configure_file("config.cmake.in" "firestore_client-config.cmake" @ONLY)
configure_file("config-version.cmake.in"
    "firestore_client-config-version.cmake" @ONLY)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/firestore_client-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
