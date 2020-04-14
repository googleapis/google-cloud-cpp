# ~~~
# Copyright 2020 Google Inc.
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

# For the Clang compiler families, enable a UBSan build type.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # UBSan build type
    set(CMAKE_CXX_FLAGS_UBSAN
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fno-omit-frame-pointer"
        CACHE STRING "Flags used by the C++ compiler during UBSan builds."
              FORCE)

    # A bit of a hack: we should not assume the C compiler also supports these
    # flags
    set(CMAKE_C_FLAGS_UBSAN
        "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined -fno-omit-frame-pointer"
        CACHE STRING "Flags used by the C compiler during UBSan builds." FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_UBSAN
        "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=undefined"
        CACHE STRING "Flags used for linking binaries during UBSan builds."
              FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_UBSAN
        "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=undefined"
        CACHE STRING
              "Flags used by the shared libraries linker during UBSan builds."
              FORCE)
    mark_as_advanced(
        CMAKE_CXX_FLAGS_UBSAN CMAKE_C_FLAGS_UBSAN CMAKE_EXE_LINKER_FLAGS_UBSAN
        CMAKE_SHARED_LINKER_FLAGS_UBSAN)
    set(CMAKE_BUILD_TYPE
        "${CMAKE_BUILD_TYPE}"
        CACHE
            STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Coverage UBSan."
            FORCE)
endif ()
