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

# Configure the Abseil dependency, this can be found as a submodule, package, or
# installed with pkg-config support.
set(GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER "module"
        CACHE STRING "How to find the Abseil library")
set_property(CACHE GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER
        PROPERTY STRINGS "module" "package" "pkg-config")

# Define the list of Abseil libraries that we might use in google-cloud-cpp.
set(ABSEIL_LIBS time dynamic_annotations spinlock_wait stacktrace int128 base
        strings)

if ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "module")
    if (NOT "${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "module")
        message(ERROR "Both Abseil and cctz must be submodules or both must"
                " be installed libraries.  Currently cctz is configured as "
                ${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}
                " and Abseil as " ${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}
                ". Consider installing Abseil too.")
    endif ()
    if (NOT ABSEIL_ROOT_DIR)
        set(ABSEIL_ROOT_DIR ${PROJECT_SOURCE_DIR}/third_party/abseil)
    endif ()
    if (NOT EXISTS "${ABSEIL_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "expected a CMakeLists.txt in ABSEIL_ROOT_DIR.")
    endif ()

    if (NOT TARGET gtest)
        # Normally the gtest target is defined by gRPC, if it is not, then
        # provide our own definition.
        include(${PROJECT_SOURCE_DIR}/cmake/IncludeGTest.cmake)
    endif ()
    add_subdirectory(${ABSEIL_ROOT_DIR} third_party/abseil EXCLUDE_FROM_ALL)

    if(MSVC)
        target_compile_definitions(absl::base PUBLIC -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
    endif(MSVC)
elseif ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "vcpkg")
    if (NOT "${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "vcpkg")
        message(ERROR "If Abseil is provided by vcpkg then cctz must also use"
                " vcpkg.  Currently cctz is configured as "
                ${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}
                " and Abseil as " ${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}
                ".  Consider using vcpkg for cctz also.")
    endif ()
    find_package(unofficial-abseil REQUIRED)
    foreach(LIB ${ABSEIL_LIBS})
        add_library(absl::${LIB} INTERFACE IMPORTED)
        set_property(TARGET absl::${LIB} PROPERTY INTERFACE_LINK_LIBRARIES
                unofficial::abseil::${LIB})
    endforeach ()
elseif ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "package")
    if ("${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "module")
        message(ERROR "Both Abseil and cctz must be submodules or both must"
                " be installed libraries.  Currently cctz is configured as "
                ${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}
                " and Abseil as " ${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}
                ".  Consider installing cctz too.")
    endif ()
    find_package(absl REQUIRED)
elseif ("${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}" STREQUAL "pkg-config")
    if ("${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}" STREQUAL "module")
        message(ERROR "Both Abseil and cctz must be submodules or both must"
                " be installed libraries.  Currently cctz is configured as "
                ${GOOGLE_CLOUD_CPP_CCTZ_PROVIDER}
                " and Abseil as " ${GOOGLE_CLOUD_CPP_ABSEIL_PROVIDER}
                ".  Consider installing cctz too.")
    endif ()
    # Use pkg-config to find the libraries.
    include(FindPkgConfig)
    foreach(LIB ${ABSEIL_LIBS})
        pkg_check_modules(absl_${LIB} REQUIRED IMPORTED_TARGET absl_${LIB})
        add_library(absl::${LIB} INTERFACE IMPORTED)
        set_property(TARGET absl::${LIB} PROPERTY INTERFACE_LINK_LIBRARIES
                PkgConfig::absl_${LIB})
    endforeach ()
endif ()
