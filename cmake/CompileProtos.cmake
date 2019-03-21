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

# Generate C++ for .proto files preserving the directory hierarchy
function (PROTOBUF_GENERATE_CPP SRCS HDRS)
    if (NOT ARGN)
        message(
            SEND_ERROR
                "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
        return()
    endif ()

    if (DEFINED PROTOBUF_IMPORT_DIRS)
        foreach (DIR ${PROTOBUF_IMPORT_DIRS})
            get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
            list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
            if (${_contains_already} EQUAL -1)
                list(APPEND _protobuf_include_path -I ${ABS_PATH})
            endif ()
        endforeach ()
    endif ()

    set(${SRCS})
    set(${HDRS})
    foreach (FIL ${ARGN})
        get_filename_component(DIR ${FIL} DIRECTORY)
        get_filename_component(FIL_WE ${FIL} NAME_WE)

        # Strip all the prefixes in ${PROTOBUF_IMPORT_DIRS} from the source
        # proto directory
        set(D ${DIR})
        if (DEFINED PROTOBUF_IMPORT_DIRS)
            foreach (P ${PROTOBUF_IMPORT_DIRS})
                string(REGEX
                       REPLACE "^${P}"
                               ""
                               T
                               "${D}")
                set(D ${T})
            endforeach ()
        endif ()
        set(SRC "${CMAKE_CURRENT_BINARY_DIR}/${D}/${FIL_WE}.pb.cc")
        set(HDR "${CMAKE_CURRENT_BINARY_DIR}/${D}/${FIL_WE}.pb.h")
        list(APPEND ${SRCS} ${SRC})
        list(APPEND ${HDRS} ${HDR})
        add_custom_command(
            OUTPUT ${SRC} ${HDR}
            COMMAND $<TARGET_FILE:protobuf::protoc>
                    ARGS
                    --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
                              ${_protobuf_include_path} ${FIL}
            DEPENDS ${FIL} protobuf::protoc
            COMMENT "Running C++ protocol buffer compiler on ${FIL}"
            VERBATIM)
    endforeach ()

    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction ()

# Generate gRPC  C++ code from .profo files, preserving the directory hierarchy.
# See https://github.com/coryan/jaybeams/issues/158 for my rant on the subject.
function (GRPC_GENERATE_CPP SRCS HDRS)
    grpc_generate_cpp_base(${SRCS} ${HDRS} UNUSED False ${ARGN})
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction ()

# Generate gRPC C++ mock classes from .proto files, preserving the directory
# hierarchy. See https://github.com/coryan/jaybeams/issues/158 for my rant on
# the subject.
function (GRPC_GENERATE_CPP_MOCKS SRCS HDRS MHDRS)
    grpc_generate_cpp_base(${SRCS} ${HDRS} ${MHDRS} True ${ARGN})
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
    set(${MHDRS} ${${MHDRS}} PARENT_SCOPE)
endfunction ()

# Refactor common sections of GRPC_GENERATE_CPP_MOCKS() and GRPC_GENERATE_CPP().
function (GRPC_GENERATE_CPP_BASE SRCS HDRS MHDRS WITH_MOCK)
    if (NOT ARGN)
        message(
            SEND_ERROR
                "Error: GRPC_GENERATE_CPP_BASE() called without any proto files"
            )
        return()
    endif ()

    set(GRPC_OUT_EXTRA)
    if (${WITH_MOCK})
        set(GRPC_OUT_EXTRA "generate_mock_code=true:")
    endif ()

    if (DEFINED PROTOBUF_IMPORT_DIRS)
        foreach (DIR ${PROTOBUF_IMPORT_DIRS})
            get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
            list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
            if (${_contains_already} EQUAL -1)
                list(APPEND _protobuf_include_path -I ${ABS_PATH})
            endif ()
        endforeach ()
    endif ()

    set(${SRCS})
    set(${HDRS})
    set(${MHDRS})
    foreach (FIL ${ARGN})
        get_filename_component(DIR ${FIL} DIRECTORY)
        get_filename_component(FIL_WE ${FIL} NAME_WE)

        # Strip all the prefixes in ${PROTOBUF_IMPORT_DIRS} from the source
        # proto directory
        set(D ${DIR})
        if (DEFINED PROTOBUF_IMPORT_DIRS)
            foreach (P ${PROTOBUF_IMPORT_DIRS})
                string(REGEX
                       REPLACE "^${P}"
                               ""
                               T
                               "${D}")
                set(D ${T})
            endforeach ()
        endif ()
        set(SRC "${CMAKE_CURRENT_BINARY_DIR}/${D}/${FIL_WE}.grpc.pb.cc")
        set(HDR "${CMAKE_CURRENT_BINARY_DIR}/${D}/${FIL_WE}.grpc.pb.h")
        set(MHDR "${CMAKE_CURRENT_BINARY_DIR}/${D}/${FIL_WE}_mock.grpc.pb.h")
        list(APPEND ${SRCS} "${SRC}")
        list(APPEND ${HDRS} "${HDR}")
        list(APPEND ${MHDRS} "${MHDR}")
        add_custom_command(
            OUTPUT "${SRC}" "${HDR}"
            COMMAND
                $<TARGET_FILE:protobuf::protoc>
                ARGS
                --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                --grpc_out=${GRPC_OUT_EXTRA}${CMAKE_CURRENT_BINARY_DIR}
                --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${_protobuf_include_path}
                                                      ${FIL}
            DEPENDS ${FIL} protobuf::protoc gRPC::grpc_cpp_plugin
            COMMENT "Running gRPC++ protocol buffer compiler on ${FIL}"
            VERBATIM)
    endforeach ()

    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
    if (${WITH_MOCK})
        set_source_files_properties(${${MHDRS}} PROPERTIES GENERATED TRUE)
        set(${MHDRS} ${${MHDRS}} PARENT_SCOPE)
    endif ()
endfunction ()
