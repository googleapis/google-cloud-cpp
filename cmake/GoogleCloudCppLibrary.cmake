# ~~~
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# A function to add targets for GA libraries that use gRPC for transport.
#
# * library:      the short name of the library, e.g. `kms`.
# * display_name: the display name of the library, e.g. "Cloud Key Management
#   Service (KMS) API"
#
# Additionally, we must set the following **variable** in the parent scope. We
# cannot use a `cmake_parse_arguments()` keyword because it will skip the empty
# string when provided in a list. We often need to use the empty string.
#
# * GOOGLE_CLOUD_CPP_SERVICE_DIRS: a list of service directories within the
#   library.
#
# The following **keywords** can be optionally supplied to handle edge cases:
#
# * ADDITIONAL_PROTO_LISTS: a list of proto files that may be used indirectly.
#   `asset` sets this.
# * BACKWARDS_COMPAT_PROTO_TARGETS: a list of proto library names (e.g.
#   `cloud_speech_protos`) that must continue to exist. We add interface
#   libraries for these, which link to the desired proto library. See #8022 for
#   more details.
# * CROSS_LIB_DEPS: a list of client libraries which this library depends on.
#
function (google_cloud_cpp_add_ga_grpc_library library display_name)
    cmake_parse_arguments(
        _opt "EXPERIMENTAL" ""
        "ADDITIONAL_PROTO_LISTS;BACKWARDS_COMPAT_PROTO_TARGETS;CROSS_LIB_DEPS"
        ${ARGN})
    set(library_target "google_cloud_cpp_${library}")
    set(mocks_target "google_cloud_cpp_${library}_mocks")
    set(protos_target "google_cloud_cpp_${library}_protos")
    set(library_alias "google-cloud-cpp::${library}")
    if (_opt_EXPERIMENTAL)
        set(library_alias "google-cloud-cpp::experimental-${library}")
    endif ()

    include(GoogleapisConfig)
    set(DOXYGEN_PROJECT_NAME "${display_name} C++ Client")
    set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the ${display_name}")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    if (_opt_EXPERIMENTAL)
        set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Experimental)")
    endif ()
    set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
    set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")
    set(GOOGLE_CLOUD_CPP_DOXYGEN_EXTRA_INCLUDES "${_opt_CROSS_LIB_DEPS}")
    list(TRANSFORM GOOGLE_CLOUD_CPP_DOXYGEN_EXTRA_INCLUDES
         PREPEND "${PROJECT_BINARY_DIR}/google/cloud/")

    unset(mocks_globs)
    unset(source_globs)
    foreach (dir IN LISTS GOOGLE_CLOUD_CPP_SERVICE_DIRS)
        if ("${dir}" STREQUAL "__EMPTY__")
            set(dir "")
        endif ()
        string(REPLACE "/" "_" ns "${dir}")
        list(APPEND source_globs "${dir}*.h" "${dir}*.cc" "${dir}internal/*")
        list(APPEND mocks_globs "${dir}mocks/*.h")
        list(APPEND DOXYGEN_EXCLUDE_SYMBOLS "${library}_${ns}internal")
        if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
            list(APPEND DOXYGEN_EXAMPLE_PATH
                 "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
        endif ()
    endforeach ()

    include(GoogleCloudCppDoxygen)
    google_cloud_cpp_doxygen_targets("${library}" DEPENDS cloud-docs
                                     "google-cloud-cpp::${library}_protos")

    include(GoogleCloudCppCommon)

    include(CompileProtos)
    google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
    google_cloud_cpp_load_protolist(
        proto_list
        "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/${library}.list")
    if (_opt_ADDITIONAL_PROTO_LISTS)
        list(APPEND proto_list "${_opt_ADDITIONAL_PROTO_LISTS}")
    endif ()
    google_cloud_cpp_load_protodeps(
        proto_deps
        "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/${library}.deps")
    google_cloud_cpp_grpcpp_library(
        ${protos_target} # cmake-format: sort
        ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
        "${PROTO_INCLUDE_DIR}")
    external_googleapis_set_version_and_alias(${library}_protos)
    target_link_libraries(${protos_target} PUBLIC ${proto_deps})

    # We used to offer the proto library by another name. Maintain backwards
    # compatibility by providing an interface library with that name. Also make
    # sure we install it as part of google_cloud_cpp_${library}-targets.
    unset(backwards_compat_proto_targets)
    foreach (old_protos IN LISTS _opt_BACKWARDS_COMPAT_PROTO_TARGETS)
        google_cloud_cpp_backwards_compat_protos_library("${old_protos}"
                                                         "${library}_protos")
        list(APPEND backwards_compat_proto_targets
             "google_cloud_cpp_${old_protos}")
    endforeach ()

    file(
        GLOB source_files
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${source_globs})
    list(SORT source_files)
    add_library(${library_target} ${source_files})
    target_include_directories(
        ${library_target}
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_link_libraries(
        ${library_target}
        PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
               google-cloud-cpp::${library}_protos)
    google_cloud_cpp_add_common_options(${library_target})
    set_target_properties(
        ${library_target}
        PROPERTIES EXPORT_NAME ${library_alias}
                   VERSION "${PROJECT_VERSION}"
                   SOVERSION "${PROJECT_VERSION_MAJOR}")
    target_compile_options(${library_target}
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    add_library(${library_alias} ALIAS ${library_target})

    # Create a header-only library for the mocks. We use a CMake `INTERFACE`
    # library for these, a regular library would not work on macOS (where the
    # library needs at least one .o file). Unfortunately INTERFACE libraries are
    # a bit weird in that they need absolute paths for their sources.
    file(
        GLOB relative_mock_files
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${mocks_globs})
    list(SORT relative_mock_files)
    set(mock_files)
    foreach (file IN LISTS relative_mock_files)
        list(APPEND mock_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
    endforeach ()
    add_library(${mocks_target} INTERFACE)
    target_sources(${mocks_target} INTERFACE ${mock_files})
    target_link_libraries(
        ${mocks_target} INTERFACE ${library_alias} GTest::gmock_main
                                  GTest::gmock GTest::gtest)
    set_target_properties(${mocks_target} PROPERTIES EXPORT_NAME
                                                     ${library_alias}_mocks)
    target_include_directories(
        ${mocks_target}
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(${mocks_target}
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    # Get the destination directories based on the GNU recommendations.
    include(GNUInstallDirs)

    # Export the CMake targets to make it easy to create configuration files.
    install(
        EXPORT ${library_target}-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${library_target}"
        COMPONENT google_cloud_cpp_development)

    # Install the libraries and headers in the locations determined by
    # GNUInstallDirs
    install(
        TARGETS ${library_target} ${protos_target}
                ${backwards_compat_proto_targets}
        EXPORT ${library_target}-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT google_cloud_cpp_runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT google_cloud_cpp_runtime
                NAMELINK_COMPONENT google_cloud_cpp_development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT google_cloud_cpp_development)

    google_cloud_cpp_install_proto_library_protos(
        "${protos_target}" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
    google_cloud_cpp_install_proto_library_headers("${protos_target}")
    google_cloud_cpp_install_headers("${library_target}"
                                     "include/google/cloud/${library}")
    google_cloud_cpp_install_headers("${mocks_target}"
                                     "include/google/cloud/${library}")

    google_cloud_cpp_add_pkgconfig(
        ${library} "The ${display_name} C++ Client Library"
        "Provides C++ APIs to use the ${display_name}"
        "google_cloud_cpp_grpc_utils" "${protos_target}")

    # Create and install the CMake configuration files.
    include(CMakePackageConfigHelpers)
    set(GOOGLE_CLOUD_CPP_CONFIG_LIBRARY "${library_target}")
    set(find_dependencies "${_opt_CROSS_LIB_DEPS}")
    list(TRANSFORM find_dependencies
         PREPEND "find_dependency(google_cloud_cpp_")
    list(TRANSFORM find_dependencies APPEND ")")
    string(JOIN "\n" GOOGLE_CLOUD_CPP_ADDITIONAL_FIND_DEPENDENCIES
           ${find_dependencies})
    configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.cmake.in"
                   "${library_target}-config.cmake" @ONLY)
    write_basic_package_version_file(
        "${library_target}-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion)

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${library_target}-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${library_target}-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${library_target}"
        COMPONENT google_cloud_cpp_development)

    external_googleapis_install_pc("${protos_target}")

    # ${library_alias} must be defined before we can add the samples.
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        foreach (dir IN LISTS GOOGLE_CLOUD_CPP_SERVICE_DIRS)
            if ("${dir}" STREQUAL "__EMPTY__")
                set(dir "")
            endif ()
            google_cloud_cpp_add_samples_relative("${library}" "${dir}samples/")
        endforeach ()
    endif ()
endfunction ()
