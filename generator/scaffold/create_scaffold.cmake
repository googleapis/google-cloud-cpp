# ~~~
# Copyright 2021 Google LLC
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

# A CMake script to generate a new library scaffold. Run using cmake
# -DCOPYRIGHT_YEAR=... -DNAME=<...> -DSERVICE_NAME="..." -P
# create_scaffold.cmake

set(PROTO_FILE_LIST)
set(PROTO_DEP_LIST)

file(READ "external/googleapis/protolists/${NAME}.list" contents)
string(REGEX REPLACE "\n" ";" contents "${contents}")
foreach (line IN LISTS contents)
    string(REPLACE "@com_google_googleapis//" "" line "${line}")
    string(REPLACE ":" "/" line "${line}")
    if ("${line}" STREQUAL "")
        continue()
    endif ()
    list(APPEND PROTO_FILE_LIST "\${EXTERNAL_GOOGLEAPIS_SOURCE}/${line}")
endforeach ()

file(READ "external/googleapis/protodeps/${NAME}.deps" contents)
string(REGEX REPLACE "\n" ";" contents "${contents}")
foreach (line IN LISTS contents)
    string(REPLACE "@com_google_googleapis//" "" line "${line}")
    if ("${line}" STREQUAL "")
        continue()
    endif ()
    string(REPLACE ":" "_" line "${line}")
    string(REPLACE "/" "_" line "${line}")
    string(REPLACE "google_" "google-cloud-cpp::" line "${line}")
    string(REPLACE "_proto" "_protos" line "${line}")
    list(APPEND PROTO_DEP_LIST "${line}")
endforeach ()

list(JOIN PROTO_FILE_LIST " " PROTO_FILES)
list(JOIN PROTO_DEP_LIST " " PROTO_DEPS)

configure_file(${CMAKE_CURRENT_LIST_DIR}/README.md.in
               google/cloud/${NAME}/README.md @ONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt.in
               google/cloud/${NAME}/CMakeLists.txt @ONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/placeholder.cc.in
               google/cloud/${NAME}/internal/placeholder.cc @ONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/config.pc.in
               google/cloud/${NAME}/config.pc.in COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/config.cmake.in
               google/cloud/${NAME}/config.cmake.in COPYONLY)

find_program(cmake_format "cmake-format")
if (NOT cmake_format)
    message("Remember to reformat google/cloud/${NAME}/CMakeLists.txt")
else ()
    execute_process(COMMAND "${cmake_format}" "-i"
                            "google/cloud/${NAME}/CMakeLists.txt")
endif ()
