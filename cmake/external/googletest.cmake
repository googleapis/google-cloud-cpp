# ~~~
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
# ~~~

include(ExternalProjectHelper)

if (NOT TARGET googletest_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(
        GOOGLE_CLOUD_CPP_GOOGLETEST_URL
        "https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.tar.gz"
        )
    set(GOOGLE_CLOUD_CPP_GOOGLETEST_SHA256
        "8d9aa381a6885fe480b7d0ce8ef747a0b8c6ee92f99d74ab07e3503434007cb0")

    set_external_project_build_parallel_level(PARALLEL)

    create_external_project_library_byproduct_list(googletest_byproducts
                                                   "gtest"
                                                   "gtest_main"
                                                   "gmock"
                                                   "gmock_main")

    include(ExternalProject)
    externalproject_add(googletest_project
                        EXCLUDE_FROM_ALL ON
                        PREFIX "${CMAKE_BINARY_DIR}/external/googletest"
                        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
                        URL ${GOOGLE_CLOUD_CPP_GOOGLETEST_URL}
                        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLETEST_SHA256}
                        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                                   $<$<BOOL:${GOOGLE_CLOUD_CPP_USE_LIBCXX}>:
                                   -DCMAKE_CXX_FLAGS=-stdlib=libc++
                                   -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-lc++abi
                                   >
                        BUILD_COMMAND ${CMAKE_COMMAND}
                                      --build
                                      <BINARY_DIR>
                                      ${PARALLEL}
                        BUILD_BYPRODUCTS ${googletest_byproducts}
                        LOG_DOWNLOAD ON
                        LOG_CONFIGURE ON
                        LOG_BUILD ON
                        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies googletest_project)
    endif ()

    # On Windows GTest uses library postfixes for debug versions, that is
    # gtest.lib becomes gtestd.lib when compiled with for debugging.  This ugly
    # expression computes that value. Note that it must be a generator
    # expression because with MSBuild the config type can change after the
    # configuration phase. We also set this for Linux since GTest will set a
    # debug postfix regardless of platform.
    if ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio.*$")
        set(_lib_postfix $<$<CONFIG:DEBUG>:d>)
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(_lib_postfix "d")
    else()
        set(_lib_postfix "")
    endif ()

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
endif ()
