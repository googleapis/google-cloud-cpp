# ~~~
# Copyright 2020, Google Inc.
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

# This file creates the skeleton of directory to hold the protos from the
# github.com/googleapis/googleapis repo. The directory skeleton will contain a
# top-level CMakeLists.txt file and a top-level cmake/ directory containing a
# few needed support files. The source of these files is in the googleapis/*
# directory that lives in this dir. Once the directory skeleton is set up, we
# add simply add it to this cmake build with add_subdirectory(), and then its
# CMakeLists.txt file will take over and handle downloading and compiling the
# actual proto files.

file(COPY "${CMAKE_CURRENT_LIST_DIR}/googleapis/CMakeLists.txt"
    DESTINATION "${CMAKE_BINARY_DIR}/external/googleapis-src")
file(COPY
    "${CMAKE_CURRENT_LIST_DIR}/googleapis/config.cmake.in"
    "${CMAKE_CURRENT_LIST_DIR}/googleapis/config.pc.in"
    "${CMAKE_CURRENT_LIST_DIR}/googleapis/config-version.cmake.in"
    DESTINATION "${CMAKE_BINARY_DIR}/external/googleapis-src/cmake")

add_subdirectory("${CMAKE_BINARY_DIR}/external/googleapis-src")

