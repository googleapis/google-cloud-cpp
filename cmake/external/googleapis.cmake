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
find_package(Threads REQUIRED)

if (NOT TARGET grpc_project)
    add_custom_target(grpc_project)
endif ()

if (NOT TARGET googleapis_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL
        "https://github.com/googleapis/cpp-cmakefiles/archive/v0.1.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
        "e8143e9132a57bd868f3014c0bc382205885c53a2a425b3c2dbb8f9feaa8366c")

    set_external_project_build_parallel_level(PARALLEL)

    set_external_project_prefix_vars()

    create_external_project_library_byproduct_list(
        googleapis_byproducts
        "googleapis_cpp_api_http_protos"
        "googleapis_cpp_api_annotations_protos"
        "googleapis_cpp_api_auth_protos"
        "googleapis_cpp_api_resource_protos"
        "googleapis_cpp_type_expr_protos"
        "googleapis_cpp_iam_v1_policy_protos"
        "googleapis_cpp_iam_v1_iam_policy_protos"
        "googleapis_cpp_rpc_error_details_protos"
        "googleapis_cpp_rpc_status_protos"
        "googleapis_cpp_longrunning_operations_protos"
        "googleapis_cpp_bigtable_protos"
        "googleapis_cpp_spanner_protos")

    set(_googleapis_toolchain_flag "")
    if (NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        set(_googleapis_toolchain_flag
            "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif ()
    set(_googleapis_triplet_flag "")
    if (NOT "${VCPKG_TARGET_TRIPLET}" STREQUAL "")
        set(_googleapis_triplet_flag
            "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
    endif ()

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
        googleapis_project
        DEPENDS grpc_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/googleapis"
        INSTALL_DIR "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        URL ${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256}
        LIST_SEPARATOR |
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND}
            -G${CMAKE_GENERATOR}
            ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
            -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
            ${_googleapis_toolchain_flag}
            ${_googleapis_triplet_flag}
            -H<SOURCE_DIR>
            -B<BINARY_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${googleapis_byproducts}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    unset(_googleapis_toolchain_flag)
    unset(_googleapis_triplet_flag)

    externalproject_get_property(googleapis_project BINARY_DIR)
    install(SCRIPT "${BINARY_DIR}/cmake_install.cmake")
    unset(BINARY_DIR)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies googleapis_project)
    endif ()
endif ()

function (googleapis_project_create_lib lib)
    set(scoped_name "googleapis-c++::${lib}_protos")
    set(imported_name "googleapis_cpp_${lib}_protos")
    if (NOT TARGET ${scoped_name})
        add_library(${scoped_name} INTERFACE IMPORTED)
        set_library_properties_for_external_project(${scoped_name}
                                                    ${imported_name})
        add_dependencies(${scoped_name} googleapis_project)
    endif ()
endfunction ()

function (gooogleapis_project_create_all_libraries)
    foreach (lib
             api_http
             api_annotations
             api_auth
             api_resource
             type_expr
             rpc_status
             rpc_error_details
             longrunning_operations
             iam_v1_policy
             iam_v1_iam_policy
             bigtable
             spanner)
        googleapis_project_create_lib(${lib})
    endforeach ()

    # We just magically "know" the dependencies between these libraries.
    set_property(TARGET googleapis-c++::api_auth_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos)
    set_property(TARGET googleapis-c++::api_annotations_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::api_http_protos)
    set_property(TARGET googleapis-c++::iam_v1_iam_policy_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::iam_v1_policy_protos
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos
                          googleapis-c++::api_resource_protos
                          googleapis-c++::type_expr_protos)
    set_property(TARGET googleapis-c++::rpc_status_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::rpc_error_details_protos)
    set_property(TARGET googleapis-c++::longrunning_operations_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::rpc_status_protos
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos)

    set_property(TARGET googleapis-c++::spanner_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::longrunning_operations_protos
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos
                          googleapis-c++::iam_v1_policy_protos
                          googleapis-c++::iam_v1_iam_policy_protos
                          googleapis-c++::rpc_status_protos)

    set_property(TARGET googleapis-c++::bigtable_protos
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          googleapis-c++::longrunning_operations_protos
                          googleapis-c++::iam_v1_policy_protos
                          googleapis-c++::api_auth_protos
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos
                          googleapis-c++::iam_v1_iam_policy_protos
                          googleapis-c++::rpc_status_protos)

    foreach (lib
             api_http
             api_annotations
             api_auth
             rpc_status
             rpc_error_details
             longrunning_operations
             iam_v1_policy
             iam_v1_iam_policy
             bigtable
             spanner)
        set(scoped_name "googleapis-c++::${lib}_protos")
        set_property(TARGET ${scoped_name}
                     APPEND
                     PROPERTY INTERFACE_LINK_LIBRARIES gRPC::grpc++
                              protobuf::libprotobuf)
    endforeach ()
endfunction ()

gooogleapis_project_create_all_libraries()
