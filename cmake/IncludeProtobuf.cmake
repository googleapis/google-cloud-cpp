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

# protobuf always requires thread support.
find_package(Threads REQUIRED)

# Configure the protobuf dependency, this can be found as a external project,
# package, or  installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER ${GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER}
    CACHE STRING "How to find protobuf libraries and compiler")
set_property(CACHE GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER
             PROPERTY STRINGS
                      "external"
                      "package"
                      "pkg-config")

function (define_protobuf_protoc_target)
    if (NOT TARGET protobuf::protoc)
        # Discover the protobuf compiler and the gRPC plugin.
        find_program(
            PROTOBUF_PROTOC_EXECUTABLE
            NAMES protoc
            DOC "The Google Protocol Buffers Compiler"
            PATHS
                ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Release
                ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${_PROTOBUF_ARCH_DIR}Debug
            )
        mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
        add_executable(protobuf::protoc IMPORTED)
        set_property(TARGET protobuf::protoc
                     PROPERTY IMPORTED_LOCATION ${PROTOBUF_PROTOC_EXECUTABLE})
    endif ()
endfunction ()

if ("${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "external")
    include(external/protobuf)
elseif("${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "package")
    find_package(protobuf REQUIRED protobuf>=3.5)

    # Older versions of CMake (<= 3.9) do not define the protobuf::protoc
    # target, in this case we define it ourselves to simplify how the rest of
    # the build scripts are constructed.
    define_protobuf_protoc_target()
elseif("${GOOGLE_CLOUD_CPP_PROTOBUF_PROVIDER}" STREQUAL "pkg-config")
    # Use pkg-config to find the libraries.
    find_package(PkgConfig REQUIRED)

    # We need a helper function to convert pkg-config(1) output into target
    # properties.
    include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigHelper.cmake)

    pkg_check_modules(Protobuf REQUIRED protobuf>=3.5)
    add_library(protobuf::libprotobuf INTERFACE IMPORTED)
    set_library_properties_from_pkg_config(protobuf::libprotobuf Protobuf)
    set_property(TARGET protobuf::libprotobuf
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
    list(APPEND PROTOBUF_IMPORT_DIRS ${Protobuf_INCLUDE_DIRS})

    # Discover the protobuf compiler and the gRPC plugin.
    define_protobuf_protoc_target()
endif ()
