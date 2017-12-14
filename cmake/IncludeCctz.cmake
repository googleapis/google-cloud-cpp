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

set(GOOGLE_CLOUD_CPP_CCTZ_PROVIDER "module" CACHE STRING "How to find the cctz libraries")
set_property(CACHE GOOGLE_CLOUD_CPP_CCTZ_PROVIDER PROPERTY STRINGS "module" "package")

if ("${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "module")
    if (NOT CCTZ_ROOT_DIR)
        set(CCTZ_ROOT_DIR ${PROJECT_SOURCE_DIR}/third_party/cctz)
    endif ()
    if (NOT EXISTS "${CCTZ_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "GOOGLE_CLOUD_CPP_CCTZ_PROVIDER is \"module\" but CCTZ_ROOT_DIR is wrong")
    endif ()
    add_subdirectory(${CCTZ_ROOT_DIR} third_party/cctz)
    set(CCTZ_LIBRARIES cctz)
    set(CCTZ_INCLUDE_DIRS ${CCTZ_ROOT_DIR}/absl)
elseif ("${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "package")
    if (WIN32)
        # On Windows we will probably use the vcpkg port (github.com/Microsoft/vcpkg).
        message(ERROR "TODO() - configure cctz under Windows")
    else ()
        # Use pkg-config on Unix and macOS.
        include(FindPkgConfig)
        pkg_check_modules(CCTZ REQUIRED cctz)
        link_directories(${CCTZ_LIBRARY_DIRS})
    endif ()
endif ()
