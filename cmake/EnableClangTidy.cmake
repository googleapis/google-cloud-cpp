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

option(GOOGLE_CLOUD_CPP_CLANG_TIDY
       "If set compiles the Cloud Cloud C++ Libraries with clang-tidy." "")

if (${CMAKE_VERSION} VERSION_LESS "3.6")
    message(STATUS "clang-tidy is not enabled because cmake version is too old")
else()
    if (${CMAKE_VERSION} VERSION_LESS "3.8")
        message(WARNING "clang-tidy exit code ignored in this version of cmake")
    endif ()
    find_program(CLANG_TIDY_EXE
                 NAMES "clang-tidy"
                 DOC "Path to clang-tidy executable")

    if (NOT CLANG_TIDY_EXE)
        message(STATUS "clang-tidy not found.")
    else()
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    endif ()
endif ()
