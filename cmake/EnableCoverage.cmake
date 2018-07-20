# ~~~
# Copyright 2017 Google Inc.
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

# For the GCC and Clang compiler families, enable a Coverage build type.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(Clang|GNU)$")

    # But only if the compiler supports the --coverage flag.  Older versions of
    # these compilers did not support it.
    set(OLD_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} --coverage")
    check_cxx_compiler_flag("--coverage" CXX_SUPPORTS_COVERAGE_FLAG)
    set(CMAKE_REQUIRED_FLAGS "${OLD_CMAKE_REQUIRED_FLAGS}")
    if (CXX_SUPPORTS_COVERAGE_FLAG)

        # Coverage build type
        set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} --coverage"
            CACHE STRING
                  "Flags used by the C++ compiler during coverage builds."
            FORCE)

        # A bit of a hack: we should not assume the C compiler also supports
        # --coverage
        set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} --coverage"
            CACHE STRING "Flags used by the C compiler during coverage builds."
            FORCE)
        set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
            "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage"
            CACHE STRING
                  "Flags used for linking binaries during coverage builds."
            FORCE)
        set(
            CMAKE_SHARED_LINKER_FLAGS_COVERAGE
            "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage"
            CACHE
                STRING
                "Flags used by the shared libraries linker during coverage builds."
            FORCE)
        mark_as_advanced(CMAKE_CXX_FLAGS_COVERAGE
                         CMAKE_C_FLAGS_COVERAGE
                         CMAKE_EXE_LINKER_FLAGS_COVERAGE
                         CMAKE_SHARED_LINKER_FLAGS_COVERAGE)
        set(
            CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}"
            CACHE
                STRING
                "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Coverage."
            FORCE)
    endif (CXX_SUPPORTS_COVERAGE_FLAG)
endif ()
