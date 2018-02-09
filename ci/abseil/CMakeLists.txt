# Copyright 2018 Google Inc.
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
project(absl-installer CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(cctz)
add_subdirectory(abseil-cpp)

include(GNUInstallDirs)
install(DIRECTORY cctz/include/cctz
    DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY cctz/include/cctz
    DESTINATION include FILES_MATCHING PATTERN "*.inc")
install(DIRECTORY abseil-cpp/absl
    DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY abseil-cpp/absl
    DESTINATION include FILES_MATCHING PATTERN "*.inc")
install(FILES ${CMAKE_BINARY_DIR}/cctz/libcctz.a
    DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(ABSL_LIB_LIST
    algorithm/libabsl_algorithm.a
    base/libabsl_base.a
    base/libabsl_dynamic_annotations.a
    base/libabsl_malloc_extension.a
    base/libabsl_malloc_internal.a
    base/libabsl_spinlock_wait.a
    base/libabsl_throw_delegate.a
    container/libabsl_container.a
    container/libtest_instance_tracker_lib.a
    debugging/libabsl_debugging.a
    debugging/libabsl_leak_check.a
    debugging/libabsl_stacktrace.a
    memory/libabsl_memory.a
    meta/libabsl_meta.a
    numeric/libabsl_int128.a
    numeric/libabsl_numeric.a
    strings/libabsl_strings.a
    synchronization/libabsl_synchronization.a
    time/libabsl_time.a
    types/libabsl_any.a
    types/libabsl_bad_any_cast.a
    types/libabsl_bad_optional_access.a
    types/libabsl_optional.a
    types/libabsl_span.a
    utility/libabsl_utility.a)
set(ABSL_STATIC_LIBRARIES)
set(ABSL_PKG_CONFIG_FILES)

# Define the dependencies for the libraries that have them:
set(ABSL_DEPENDENCIES_absl_base absl_dynamic_annotations absl_spinlock_wait)
set(ABSL_DEPENDENCIES_absl_time absl_base absl_dynamic_annotations
    absl_spinlock_wait absl_stacktrace absl_int128 cctz)
set(ABSL_DEPENDENCIES_absl_strings absl_base absl_throw_delegate
    absl_dynamic_annotations absl_spinlock_wait)
set(ABSL_DEPENDENCIES_absl_malloc_extension absl_base
    absl_dynamic_annotations absl_spinlock_wait)
set(ABSL_DEPENDENCIES_absl_synchronization absl_malloc_extension asbl_time
    absl_base absl_stacktrace absl_int128
    absl_dynamic_annotations absl_spinlock_wait cctz)

foreach (LIB ${ABSL_LIB_LIST})
    list(APPEND ABSL_STATIC_LIBRARIES
        "${CMAKE_BINARY_DIR}/abseil-cpp/absl/${LIB}")
    get_filename_component(ABSL_LIBNAME ${LIB} NAME_WE)
    string(REPLACE "lib" "" ABSL_LIB "${ABSL_LIBNAME}")
    set(ABSL_DEPENDENCIES)
    foreach (DEP ${ABSL_DEPENDENCIES_${ABSL_LIB}})
        string(CONCAT ABSL_DEPENDENCIES "${ABSL_DEPENDENCIES}" " -l${DEP}")
    endforeach ()
    configure_file("absl-config.pc.in" "${ABSL_LIB}.pc" @ONLY)
    list(APPEND ABSL_PKG_CONFIG_FILES
        "${CMAKE_BINARY_DIR}/${ABSL_LIB}.pc")
endforeach ()

install(FILES ${ABSL_STATIC_LIBRARIES}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ${ABSL_PKG_CONFIG_FILES}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

configure_file("cctz.pc.in" "cctz.pc" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/cctz.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
