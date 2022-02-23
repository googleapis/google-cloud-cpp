# ~~~
# Copyright 2017, Google Inc.
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

# Introduce a new TARGET property to associate proto files with a target.
#
# We use a function to define the property so it can be called multiple times
# without introducing the property over and over.
function (google_cloud_cpp_add_protos_property)
    set_property(
        TARGET
        PROPERTY PROTO_SOURCES BRIEF_DOCS
                 "The list of .proto files for a target." FULL_DOCS
                 "List of .proto files specified for a target.")
endfunction ()

# Generate C++ for .proto files preserving the directory hierarchy
#
# Receives a list of `.proto` file names and (a) creates the runs to convert
# these files to `.pb.h` and `.pb.cc` output files, (b) returns the list of
# `.pb.cc` files and `.pb.h` files in @p HDRS, and (c) creates the list of files
# preserving the directory hierarchy, such that if a `.proto` file says:
#
# import "foo/bar/baz.proto"
#
# the resulting C++ code says:
#
# #include <foo/bar/baz.pb.h>
#
# Use the `PROTO_PATH` option to provide one or more directories to search for
# proto files in the import.
#
# If `LOCAL_INCLUDE` is set to true, `file.pb.cc` file generated for
# `a/b/file.proto` will include `file.pb.h` rather than `a/b/file.pb.h`.
#
# @par Example
#
# google_cloud_cpp_generate_proto( MY_PB_FILES "foo/bar/baz.proto"
# "foo/bar/qux.proto" PROTO_PATH_DIRECTORIES "another/dir/with/protos")
#
# Note that `protoc` the protocol buffer compiler requires your protos to be
# somewhere in the search path defined by the `--proto_path` (aka -I) options.
# For example, if you want to generate the `.pb.{h,cc}` files for
# `foo/bar/baz.proto` then the directory containing `foo` must be in the search
# path.
function (google_cloud_cpp_generate_proto SRCS)
    cmake_parse_arguments(_opt "LOCAL_INCLUDE" "" "PROTO_PATH_DIRECTORIES"
                          ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Error: google_cloud_cpp_generate_proto() called"
                           " without any proto files")
        return()
    endif ()

    # Build the list of `--proto_path` options. Use the absolute path for each
    # option given, and do not include any path more than once.
    set(protobuf_include_path)
    foreach (dir ${_opt_PROTO_PATH_DIRECTORIES})
        get_filename_component(absolute_path ${dir} ABSOLUTE)
        list(FIND protobuf_include_path "${absolute_path}"
             already_in_search_path)
        if (${already_in_search_path} EQUAL -1)
            list(APPEND protobuf_include_path "--proto_path" "${absolute_path}")
        endif ()
    endforeach ()

    set(${SRCS})
    foreach (file_path ${_opt_UNPARSED_ARGUMENTS})
        get_filename_component(file_directory "${file_path}" DIRECTORY)
        get_filename_component(file_name "${file_path}" NAME)
        # This gets the filename without the extension, analogous to $(basename
        # "${file_path}" .proto)
        get_filename_component(file_stem "${file_path}" NAME_WE)

        # Strip all the prefixes in ${_opt_PROTO_PATH_DIRECTORIES} from the
        # source proto directory
        set(D "${file_directory}")
        if (DEFINED _opt_PROTO_PATH_DIRECTORIES)
            foreach (P ${_opt_PROTO_PATH_DIRECTORIES})
                string(REGEX REPLACE "^${P}" "" T "${D}")
                set(D ${T})
            endforeach ()
        endif ()
        set(pb_cc "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.pb.cc")
        set(pb_h "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.pb.h")
        list(APPEND ${SRCS} "${pb_cc}" "${pb_h}")

        if (${_opt_LOCAL_INCLUDE})
            add_custom_command(
                OUTPUT "${pb_cc}" "${pb_h}"
                COMMAND
                    $<TARGET_FILE:protobuf::protoc> ARGS --cpp_out
                    "${CMAKE_CURRENT_BINARY_DIR}/${D}" ${protobuf_include_path}
                    "${file_name}"
                DEPENDS "${file_path}" protobuf::protoc
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${D}"
                COMMENT "Running C++ protocol buffer compiler on ${file_path}"
                VERBATIM)
        else ()
            add_custom_command(
                OUTPUT "${pb_cc}" "${pb_h}"
                COMMAND
                    $<TARGET_FILE:protobuf::protoc> ARGS --cpp_out
                    "${CMAKE_CURRENT_BINARY_DIR}" ${protobuf_include_path}
                    "${file_path}"
                DEPENDS "${file_path}" protobuf::protoc
                COMMENT "Running C++ protocol buffer compiler on ${file_path}"
                VERBATIM)
        endif ()
    endforeach ()

    set_source_files_properties(${${SRCS}} PROPERTIES GENERATED TRUE)
    set(${SRCS}
        ${${SRCS}}
        PARENT_SCOPE)
endfunction ()

# Generate gRPC C++ files from .proto files preserving the directory hierarchy.
#
# Receives a list of `.proto` file names and (a) creates the runs to convert
# these files to `.grpc.pb.h` and `.grpc.pb.cc` output files, (b) returns the
# list of `.grpc.pb.cc` and `.pb.h` files in @p SRCS, and (c) creates the list
# of files preserving the directory hierarchy, such that if a `.proto` file says
#
# import "foo/bar/baz.proto"
#
# the resulting C++ code says:
#
# #include <foo/bar/baz.pb.h>
#
# Use the `PROTO_PATH` option to provide one or more directories to search for
# proto files in the import.
#
# @par Example
#
# google_cloud_cpp_generate_grpc( MY_GRPC_PB_FILES "foo/bar/baz.proto"
# "foo/bar/qux.proto" PROTO_PATH_DIRECTORIES "another/dir/with/protos")
#
# Note that `protoc` the protocol buffer compiler requires your protos to be
# somewhere in the search path defined by the `--proto_path` (aka -I) options.
# For example, if you want to generate the `.pb.{h,cc}` files for
# `foo/bar/baz.proto` then the directory containing `foo` must be in the search
# path.
function (google_cloud_cpp_generate_grpcpp SRCS)
    cmake_parse_arguments(_opt "" "" "PROTO_PATH_DIRECTORIES" ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(
            SEND_ERROR "Error: google_cloud_cpp_generate_grpc() called without"
                       " any proto files")
        return()
    endif ()

    # Build the list of `--proto_path` options. Use the absolute path for each
    # option given, and do not include any path more than once.
    set(protobuf_include_path)
    foreach (dir ${_opt_PROTO_PATH_DIRECTORIES})
        get_filename_component(absolute_path ${dir} ABSOLUTE)
        list(FIND protobuf_include_path "${absolute_path}"
             already_in_search_path)
        if (${already_in_search_path} EQUAL -1)
            list(APPEND protobuf_include_path "--proto_path" "${absolute_path}")
        endif ()
    endforeach ()

    set(${SRCS})
    foreach (filename ${_opt_UNPARSED_ARGUMENTS})
        get_filename_component(file_directory "${filename}" DIRECTORY)
        # This gets the filename without the extension, analogous to $(basename
        # "${filename}" .proto)
        get_filename_component(file_stem "${filename}" NAME_WE)

        # Strip all the prefixes in ${_opt_PROTO_PATH_DIRECTORIES} from the
        # source proto directory
        set(D "${file_directory}")
        if (DEFINED _opt_PROTO_PATH_DIRECTORIES)
            foreach (P ${_opt_PROTO_PATH_DIRECTORIES})
                string(REGEX REPLACE "^${P}" "" T "${D}")
                set(D ${T})
            endforeach ()
        endif ()
        set(grpc_pb_cc
            "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.grpc.pb.cc")
        set(grpc_pb_h "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.grpc.pb.h")
        list(APPEND ${SRCS} "${grpc_pb_cc}" "${grpc_pb_h}")
        add_custom_command(
            OUTPUT "${grpc_pb_cc}" "${grpc_pb_h}"
            COMMAND
                $<TARGET_FILE:protobuf::protoc> ARGS
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                "--grpc_out=${CMAKE_CURRENT_BINARY_DIR}"
                "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}" ${protobuf_include_path}
                "${filename}"
            DEPENDS "${filename}" protobuf::protoc gRPC::grpc_cpp_plugin
            COMMENT "Running gRPC C++ protocol buffer compiler on ${filename}"
            VERBATIM)
    endforeach ()

    set_source_files_properties(${${SRCS}} PROPERTIES GENERATED TRUE)
    set(${SRCS}
        ${${SRCS}}
        PARENT_SCOPE)
endfunction ()

# Generate a list of proto files from a `protolists/*.list` file.
#
# The proto libraries in googleapis/googleapis do not ship with support for
# CMake. We need to write our own CMake files. To ease the maintenance effort we
# use a script that queries the BUILD files in googleapis/googleapis, and
# extracts the list of `.proto` files for each proto library we are interested
# in. These files are extracted as Bazel rule names, for example:
#
# @com_google_googleapis//google/bigtable/v2:bigtable.proto
#
# We use naming conventions to convert these rules files to a path. Using the
# same example that becomes:
#
# ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/bigtable/v2/bigtable.proto
#
function (google_cloud_cpp_load_protolist var file)
    file(READ "${file}" contents)
    string(REGEX REPLACE "\n" ";" contents "${contents}")
    set(protos)
    foreach (line IN LISTS contents)
        string(REPLACE "@com_google_googleapis//" "" line "${line}")
        string(REPLACE ":" "/" line "${line}")
        if ("${line}" STREQUAL "")
            continue()
        endif ()
        list(APPEND protos "${EXTERNAL_GOOGLEAPIS_SOURCE}/${line}")
    endforeach ()
    set(${var}
        "${protos}"
        PARENT_SCOPE)
endfunction ()

# Generate a list of proto dependencies from a `protodeps/*.deps` file.
#
# The proto libraries in googleapis/googleapis do not ship with support for
# CMake. We need to write our own CMake files. To ease the maintenance effort we
# use a script that queries the BUILD files in googleapis/googleapis, and
# extracts the list of dependencies for each proto library we are interested in.
# These dependencies are extracted as Bazel rule names, for example:
#
# @com_google_googleapis//google/api:annotations_proto
#
# We use naming conventions to convert these rules files to a CMake target.
# Using the same example that becomes:
#
# google-cloud-cpp::api_annotations_proto
#
function (google_cloud_cpp_load_protodeps var file)
    file(READ "${file}" contents)
    string(REGEX REPLACE "\n" ";" contents "${contents}")
    set(deps)
    foreach (line IN LISTS contents)
        if ("${line}" STREQUAL "")
            continue()
        endif ()
        string(REPLACE ":" "_" line "${line}")
        string(REPLACE "_proto" "_protos" line "${line}")
        string(REPLACE "@com_google_googleapis//" "google-cloud-cpp::" line
                       "${line}")
        # Avoid duplicate `google`'s in the target name.
        string(REPLACE "google-cloud-cpp::google/" "google-cloud-cpp::" line
                       "${line}")
        string(REPLACE "/" "_" line "${line}")
        list(APPEND deps "${line}")
    endforeach ()
    set(${var}
        "${deps}"
        PARENT_SCOPE)
endfunction ()

include(GNUInstallDirs)

# Install headers for a C++ proto library.
function (google_cloud_cpp_install_proto_library_headers target)
    get_target_property(target_sources ${target} SOURCES)
    foreach (header ${target_sources})
        # Skip anything that is not a header file.
        if (NOT "${header}" MATCHES "\\.h$")
            continue()
        endif ()
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" relative "${header}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(
            FILES "${header}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dir}"
            COMPONENT google_cloud_cpp_development)
    endforeach ()
endfunction ()

# Install protos for a C++ proto library.
function (google_cloud_cpp_install_proto_library_protos target source_dir)
    get_target_property(target_protos ${target} PROTO_SOURCES)
    foreach (header ${target_protos})
        # Skip anything that is not a header file.
        if (NOT "${header}" MATCHES "\\.proto$")
            continue()
        endif ()
        string(REPLACE "${source_dir}/" "" relative "${header}")
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" relative "${relative}")
        get_filename_component(dir "${relative}" DIRECTORY)
        # This is modeled after the Protobuf library, it installs the basic
        # protos (think google/protobuf/any.proto) in the include directory for
        # C/C++ code. :shrug:
        install(
            FILES "${header}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dir}"
            COMPONENT google_cloud_cpp_development)
    endforeach ()
endfunction ()

include(EnableWerror)

function (google_cloud_cpp_proto_library libname)
    cmake_parse_arguments(_opt "LOCAL_INCLUDE" "" "PROTO_PATH_DIRECTORIES"
                          ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Error: google_cloud_cpp_proto_library() called"
                           " without any proto files")
        return()
    endif ()

    if (${_opt_LOCAL_INCLUDE})
        google_cloud_cpp_generate_proto(
            proto_sources ${_opt_UNPARSED_ARGUMENTS} LOCAL_INCLUDE
            PROTO_PATH_DIRECTORIES ${_opt_PROTO_PATH_DIRECTORIES})
    else ()
        google_cloud_cpp_generate_proto(
            proto_sources ${_opt_UNPARSED_ARGUMENTS} PROTO_PATH_DIRECTORIES
            ${_opt_PROTO_PATH_DIRECTORIES})
    endif ()

    add_library(${libname} ${proto_sources})
    set_property(TARGET ${libname} PROPERTY PROTO_SOURCES
                                            ${_opt_UNPARSED_ARGUMENTS})
    target_link_libraries(${libname} PUBLIC gRPC::grpc++ gRPC::grpc
                                            protobuf::libprotobuf)
    # We want to treat the generated code as "system" headers so they get
    # ignored by the more aggressive warnings.
    target_include_directories(
        ${libname} SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                 $<INSTALL_INTERFACE:include>)
    google_cloud_cpp_silence_warnings_in_deps(${libname})
    # Disable clang-tidy for generated code, note that the CXX_CLANG_TIDY
    # property was introduced in 3.6, and we do not use clang-tidy with older
    # versions
    if (NOT ("${CMAKE_VERSION}" VERSION_LESS 3.6))
        set_target_properties(${libname} PROPERTIES CXX_CLANG_TIDY "")
    endif ()
    if (MSVC)
        # The protobuf-generated files have warnings under the default MSVC
        # settings. We are not interested in these warnings, because we cannot
        # fix them.
        target_compile_options(${libname} PRIVATE "/wd4244" "/wd4251")
    endif ()

    # In some configs we need to only generate the protocol definitions from
    # `*.proto` files. We achieve this by having this target depend on all proto
    # libraries. It has to be defined at the top level of the project.
    add_dependencies(google-cloud-cpp-protos ${libname})
endfunction ()

function (google_cloud_cpp_grpcpp_library libname)
    cmake_parse_arguments(_opt "" "" "PROTO_PATH_DIRECTORIES" ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Error: google_cloud_cpp_proto_library() called"
                           " without any proto files")
        return()
    endif ()

    google_cloud_cpp_generate_grpcpp(
        grpcpp_sources ${_opt_UNPARSED_ARGUMENTS} PROTO_PATH_DIRECTORIES
        ${_opt_PROTO_PATH_DIRECTORIES})
    google_cloud_cpp_proto_library(
        ${libname} ${_opt_UNPARSED_ARGUMENTS} PROTO_PATH_DIRECTORIES
        ${_opt_PROTO_PATH_DIRECTORIES})
    target_sources(${libname} PRIVATE ${grpcpp_sources})
endfunction ()

macro (external_googleapis_install_pc_common target)
    string(REPLACE "google_cloud_cpp_" "" _short_name ${target})
    string(REPLACE "_protos" "" _short_name ${_short_name})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "The Google APIS C++ ${_short_name} Proto Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Compiled proto for C++.")
    # Examine the target LINK_LIBRARIES property, use that to pull the
    # dependencies between the google-cloud-cpp::* libraries.
    set(_target_pc_requires)
    get_target_property(_target_deps ${target} INTERFACE_LINK_LIBRARIES)
    foreach (dep ${_target_deps})
        if ("${dep}" MATCHES "^google-cloud-cpp::")
            string(REPLACE "google-cloud-cpp::" "google_cloud_cpp_" dep
                           "${dep}")
            list(APPEND _target_pc_requires " " "${dep}")
        endif ()
    endforeach ()
    # These dependencies are required for all the google-cloud-cpp::* libraries.
    list(
        APPEND
        _target_pc_requires
        " grpc++"
        " grpc"
        " openssl"
        " protobuf"
        " zlib"
        " libcares")
    string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES ${_target_pc_requires})
    get_target_property(_target_type ${target} TYPE)
    if ("${_target_type}" STREQUAL "INTERFACE_LIBRARY")
        set(GOOGLE_CLOUD_CPP_PC_LIBS "")
    else ()
        set(GOOGLE_CLOUD_CPP_PC_LIBS "-l${target}")
    endif ()
endmacro ()

# Use a function to create a scope for the variables.
function (external_googleapis_install_pc target source_dir)
    external_googleapis_install_pc_common("${target}")
    configure_file("${source_dir}/config.pc.in" "${target}.pc" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT google_cloud_cpp_development)
endfunction ()
