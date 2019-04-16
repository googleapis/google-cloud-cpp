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
    set(
        GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL
        "https://github.com/googleapis/googleapis/archive/6a3277c0656219174ff7c345f31fb20a90b30b97.tar.gz"
        )
    set(GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256
        "793f1fe9d3adf28900792b4151b7cb2fa4ef14ae1e14ea4e7faa2be14be7a301")

    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles"
        OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        include(ProcessorCount)
        processorcount(NCPU)
        set(PARALLEL "--" "-j" "${NCPU}")
    else()
        set(PARALLEL "")
    endif ()

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH "<INSTALL_DIR>/lib;<INSTALL_DIR>/lib64")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_INSTALL_RPATH
                   "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}")

    create_external_project_library_byproduct_list(
        googleapis_byproducts
        "googleapis_cpp_api_http_protos"
        "googleapis_cpp_api_annotations_protos"
        "googleapis_cpp_api_auth_protos"
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

    include(ExternalProject)
    externalproject_add(
        googleapis_project
        DEPENDS grpc_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/googleapis"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_GOOGLEAPIS_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_GOOGLEAPIS_SHA256}
        LIST_SEPARATOR
        |
        PATCH_COMMAND
        ${CMAKE_COMMAND}
        -E
        copy
        ${PROJECT_SOURCE_DIR}/cmake/external/googleapis/CMakeLists.txt
        <SOURCE_DIR>/CMakeLists.txt
        COMMAND
            ${CMAKE_COMMAND} -E copy
            ${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake
            ${PROJECT_SOURCE_DIR}/cmake/PkgConfigHelper.cmake
            ${PROJECT_SOURCE_DIR}/cmake/FindgRPC.cmake
            ${PROJECT_SOURCE_DIR}/cmake/FindProtobufTargets.cmake
            ${PROJECT_SOURCE_DIR}/cmake/SelectMSVCRuntime.cmake
            ${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in
            ${PROJECT_SOURCE_DIR}/cmake/external/googleapis/config.cmake.in
            ${PROJECT_SOURCE_DIR}/cmake/external/googleapis/config-version.cmake.in
            <SOURCE_DIR>
        CMAKE_ARGS ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                   -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
                   -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                   -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
                   -DGOOGLE_CLOUD_CPP_USE_LIBCXX=${GOOGLE_CLOUD_CPP_USE_LIBCXX}
                   ${_googleapis_toolchain_flag}
                   ${_googleapis_triplet_flag}
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
                          googleapis-c++::api_http_protos)
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
                          googleapis-c++::api_auth_protos
                          googleapis-c++::api_annotations_protos
                          googleapis-c++::api_http_protos
                          googleapis-c++::iam_v1_policy_protos
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
