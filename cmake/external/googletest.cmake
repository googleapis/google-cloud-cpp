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
    set(GOOGLE_CLOUD_CPP_GOOGLETEST_URL
        "https://github.com/google/googletest/archive/release-1.8.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_GOOGLETEST_SHA256
        "9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c")

    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

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
                                   -DCMAKE_BUILD_TYPE=Release
                                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                        BUILD_COMMAND ${CMAKE_COMMAND}
                                      --build
                                      <BINARY_DIR>
                                      ${PARALLEL}
                        BUILD_BYPRODUCTS ${googletest_byproducts}
                        LOG_DOWNLOAD ON
                        LOG_CONFIGURE ON
                        LOG_BUILD ON
                        LOG_INSTALL ON)

    # On Windows GTest uses library postfixes for debug versions, that is
    # gtest.lib becomes gtestd.lib when compiled with for debugging.  This ugly
    # expression computes that value. Note that it must be a generator
    # expression because with MSBuild the config type can change after the
    # configuration phase.
    if (WIN32)
        set(_lib_postfix $<$<CONFIG:DEBUG>:d>)
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
