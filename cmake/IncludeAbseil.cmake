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

include(${PROJECT_SOURCE_DIR}/cmake/IncludeCctz.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/IncludeGMock.cmake)

# Depending on how gRPC is used (module vs. package), the gtest target may be
# already defined; if it is, we cannot redefine it.
if (NOT TARGET gtest)
    include(${PROJECT_SOURCE_DIR}/cmake/IncludeGTest.cmake)
endif ()

if (NOT ABSEIL_ROOT_DIR)
    set(ABSEIL_ROOT_DIR ${PROJECT_THIRD_PARTY_DIR}/abseil)
endif ()
if (NOT EXISTS "${ABSEIL_ROOT_DIR}/CMakeLists.txt")
    message(ERROR "expected a CMakeLists.txt in ABSEIL_ROOT_DIR.")
endif ()
add_subdirectory(${ABSEIL_ROOT_DIR} third_party/abseil EXCLUDE_FROM_ALL)
set(ABSEIL_LIBRARIES abseil)
set(ABSEIL_INCLUDE_DIRS ${ABSEIL_ROOT_DIR})

if(MSVC)
target_compile_definitions(absl_base PUBLIC -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif(MSVC)
