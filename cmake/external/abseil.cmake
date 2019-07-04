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

include(ExternalProjectHelper)

if (NOT TARGET abseil_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub. TODO: decide on which commit to use.
    set(
        GOOGLE_CLOUD_CPP_ABSEIL_URL
        "https://github.com/abseil/abseil-cpp/archive/74d91756c11bc22f9b0108b94da9326f7f9e376f.tar.gz"
        )
    set(GOOGLE_CLOUD_CPP_ABSEIL_SHA256
        "fd4edc10767c28b23bf9f41114c6bcd9625c165a31baa0e6939f01058029a912")

    set_external_project_build_parallel_level(PARALLEL)

    # TODO: come up with list of byproducts. Not every public target has a
    # corresponding library, and a lot of libraries are generated that do not
    # have a corresponding public target.
    create_external_project_library_byproduct_list(abseil_byproducts
                                                   "absl_base"
                                                   "absl_hash"
                                                   "absl_strings"
                                                   "absl_synchronization"
                                                   "absl_time")

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    set(GOOGLE_CLOUD_CPP_PREFIX_PATH "${CMAKE_PREFIX_PATH};<INSTALL_DIR>")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_PREFIX_PATH
                   "${GOOGLE_CLOUD_CPP_PREFIX_PATH}")

    include(ExternalProject)
    externalproject_add(
        abseil_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/abseil"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_ABSEIL_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_ABSEIL_SHA256}
        LIST_SEPARATOR |
        CMAKE_ARGS
            ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
            -DABSL_HAVE_EXCEPTIONS=${GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS}
            -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
            -DBUILD_TESTING=OFF
            -DABSL_USE_GOOGLETEST_HEAD=OFF
            -DABSL_RUN_TESTS=OFF
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${abseil_byproducts}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies abseil_project)
    endif ()

    include(ExternalProjectHelper)
    add_library(absl::base INTERFACE IMPORTED)
    add_dependencies(absl::base absl_project)
    set_library_properties_for_external_project(absl::base absl_base)

    if (MSVC)
        target_compile_definitions(absl::base
                                   PUBLIC
                                   NOMINMAX
                                   WIN32_LEAN_AND_MEAN
                                   _WIN32_WINNT=0x600
                                   _SCL_SECURE_NO_WARNINGS
                                   _CRT_SECURE_NO_WARNINGS)
    endif (MSVC)

    # TODO: match targets with corresponding libraries.
    add_library(absl::algorithm INTERFACE IMPORTED)
    add_dependencies(absl::algorithm absl_project)

    add_library(absl::debugging INTERFACE IMPORTED)
    add_dependencies(absl::debugging absl_project)

    add_library(absl::flat_hash_map INTERFACE IMPORTED)
    add_dependencies(absl::flat_hash_map absl_project)
    set_library_properties_for_external_project(absl::flat_hash_map absl_hash)

    add_library(absl::memory INTERFACE IMPORTED)
    add_dependencies(absl::memory absl_project)

    add_library(absl::meta INTERFACE IMPORTED)
    add_dependencies(absl::meta absl_project)

    add_library(absl::numeric INTERFACE IMPORTED)
    add_dependencies(absl::numeric absl_project)

    add_library(absl::strings INTERFACE IMPORTED)
    add_dependencies(absl::strings absl_project)
    set_library_properties_for_external_project(absl::strings absl_strings)

    add_library(absl::synchronization INTERFACE IMPORTED)
    add_dependencies(absl::synchronization absl_project)
    set_library_properties_for_external_project(absl::synchronization
                                                absl_synchronization)

    add_library(absl::time INTERFACE IMPORTED)
    add_dependencies(absl::time absl_project)
    set_library_properties_for_external_project(absl::time absl_time)

    add_library(absl::utility INTERFACE IMPORTED)
    add_dependencies(absl::utility absl_project)
endif ()
