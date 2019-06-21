# ~~~
# Copyright 2017, Google Inc.
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

# Introduce a new TARGET property to associate proto files with a target.
#
# We use a function to define the property so it can be called multiple times
# without introducing the property over and over.
function (google_cloud_cpp_add_protos_property)
    set_property(TARGET
                 PROPERTY PROTO_SOURCES
                          BRIEF_DOCS
                          "The list of .proto files for a target."
                          FULL_DOCS
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
    cmake_parse_arguments(_opt "" "" "PROTO_PATH_DIRECTORIES" ${ARGN})
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
                string(REGEX
                       REPLACE "^${P}"
                               ""
                               T
                               "${D}")
                set(D ${T})
            endforeach ()
        endif ()
        set(pb_cc "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.pb.cc")
        set(pb_h "${CMAKE_CURRENT_BINARY_DIR}/${D}/${file_stem}.pb.h")
        list(APPEND ${SRCS} "${pb_cc}" "${pb_h}")
        add_custom_command(
            OUTPUT "${pb_cc}" "${pb_h}"
            COMMAND $<TARGET_FILE:protobuf::protoc>
                    ARGS
                    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
                              ${protobuf_include_path} "${filename}"
            DEPENDS "${filename}" protobuf::protoc
            COMMENT "Running C++ protocol buffer compiler on ${filename}"
            VERBATIM)
    endforeach ()

    set_source_files_properties(${${SRCS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
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
                string(REGEX
                       REPLACE "^${P}"
                               ""
                               T
                               "${D}")
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
                $<TARGET_FILE:protobuf::protoc>
                ARGS
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                    "--grpc_out=${CMAKE_CURRENT_BINARY_DIR}"
                    "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}"
                    ${protobuf_include_path} "${filename}"
            DEPENDS "${filename}" protobuf::protoc gRPC::grpc_cpp_plugin
            COMMENT "Running gRPC C++ protocol buffer compiler on ${filename}"
            VERBATIM)
    endforeach ()

    set_source_files_properties(${${SRCS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
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
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/"
                       ""
                       relative
                       "${header}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(FILES "${header}" DESTINATION
                      "${CMAKE_INSTALL_INCLUDEDIR}/${dir}")
    endforeach ()
endfunction ()

# Install protos for a C++ proto library.
function (google_cloud_cpp_install_proto_library_protos target)
    get_target_property(target_protos ${target} PROTO_SOURCES)
    foreach (header ${target_protos})
        # Skip anything that is not a header file.
        if (NOT "${header}" MATCHES "\\.proto$")
            continue()
        endif ()
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/"
                       ""
                       relative
                       "${header}")
        get_filename_component(dir "${relative}" DIRECTORY)
        # This is modeled after the Protobuf library, it installs the basic
        # protos (think google/protobuf/any.proto) in the include directory for
        # C/C++ code. :shrug:
        install(FILES "${header}" DESTINATION
                      "${CMAKE_INSTALL_INCLUDEDIR}/${dir}")
    endforeach ()
endfunction ()

function (google_cloud_cpp_proto_library libname)
    cmake_parse_arguments(_opt "" "" "PROTO_PATH_DIRECTORIES" ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Error: google_cloud_cpp_proto_library() called"
                           " without any proto files")
        return()
    endif ()

    google_cloud_cpp_generate_proto(proto_sources
                                    ${_opt_UNPARSED_ARGUMENTS}
                                    PROTO_PATH_DIRECTORIES
                                    ${_opt_PROTO_PATH_DIRECTORIES})

    add_library(${libname} ${proto_sources})
    set_property(TARGET ${libname}
                 PROPERTY PROTO_SOURCES ${_opt_UNPARSED_ARGUMENTS})
    target_link_libraries(${libname}
                          PUBLIC gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
    target_include_directories(
        ${libname}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
               $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<INSTALL_INTERFACE:include>)
endfunction ()

function (google_cloud_cpp_grpcpp_library libname)
    cmake_parse_arguments(_opt "" "" "PROTO_PATH_DIRECTORIES" ${ARGN})
    if (NOT _opt_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Error: google_cloud_cpp_proto_library() called"
                           " without any proto files")
        return()
    endif ()

    google_cloud_cpp_generate_grpcpp(grpcpp_sources
                                     ${_opt_UNPARSED_ARGUMENTS}
                                     PROTO_PATH_DIRECTORIES
                                     ${_opt_PROTO_PATH_DIRECTORIES})
    google_cloud_cpp_proto_library(${libname}
                                   ${_opt_UNPARSED_ARGUMENTS}
                                   PROTO_PATH_DIRECTORIES
                                   ${_opt_PROTO_PATH_DIRECTORIES})
    target_sources(${libname} PRIVATE ${grpcpp_sources})
endfunction ()
