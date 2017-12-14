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

set(GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER "module" CACHE STRING "How to find the abseil libraries")
set_property(CACHE GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER PROPERTY STRINGS "module" "package")

if ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "module")
    if (NOT ABSEIL_ROOT_DIR)
        set(ABSEIL_ROOT_DIR ${PROJECT_SOURCE_DIR}/third_party/abseil)
    endif ()
    if (NOT EXISTS "${ABSEIL_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER is \"module\" but ABSEIL_ROOT_DIR is wrong")
    endif ()
    add_subdirectory(${ABSEIL_ROOT_DIR} third_party/abseil)
    set(ABSEIL_LIBRARIES abseil)
    set(ABSEIL_INCLUDE_DIRS ${ABSEIL_ROOT_DIR})
elseif ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "package")
    if (WIN32)
        # On Windows we will probably use the vcpkg port (github.com/Microsoft/vcpkg).
        message(ERROR "TODO() - configure abseil under Windows")
    else ()
        # Use pkg-config on Unix and macOS.
        include(FindPkgConfig)
        pkg_check_modules(ABSEIL REQUIRED abseil)
        link_directories(${ABSEIL_LIBRARY_DIRS})
    endif ()
endif ()
