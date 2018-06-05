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

# Find out the name of the subproject.
get_filename_component(GOOGLE_CLOUD_CPP_SUBPROJECT "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Find out what flags turn on all available warnings and turn those warnings
# into errors.
include(CheckCXXCompilerFlag)
if (NOT MSVC)
    CHECK_CXX_COMPILER_FLAG(-Wall GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
    CHECK_CXX_COMPILER_FLAG(-Werror GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
else ()
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
endif ()

# If possible, enable a code coverage build type.
include(EnableCoverage)

# Include the functions to enable Clang sanitizers.
include(EnableSanitizers)

# Include support for clang-tidy if available
include(EnableClangTidy)

# C++ Exceptions are enabled by default, but allow the user to turn them off.
include(EnableCxxExceptions)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

function(google_cloud_cpp_add_common_options target)
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
        target_compile_options(${target} INTERFACE "/std:c++latest")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
        target_compile_options(${target} INTERFACE "-Wall")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
        target_compile_options(${target} INTERFACE "-Werror")
    endif ()
endfunction()

function (google_cloud_cpp_add_clang_tidy target)
    if (CLANG_TIDY_EXE AND GOOGLE_CLOUD_CPP_CLANG_TIDY)
        set_target_properties(
                ${target} PROPERTIES
                CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
    endif ()
endfunction ()
