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

function (set_library_properties_for_external_project _target _lib)
    cmake_parse_arguments(F_OPT "ALWAYS_SHARED" "" "" ${ARGN})
    # This is the main disadvantage of external projects. We cannot use the
    # project's cmake configuration file because they are not installed until
    # the project is compiled, and this step
    if (WIN32)
        set(_libfullname "${CMAKE_STATIC_LIBRARY_PREFIX}${_lib}${CMAKE_LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    elseif ("${BUILD_SHARED_LIBS}" OR "${F_OPT_ALWAYS_SHARED}")
        set(_libfullname "${CMAKE_SHARED_LIBRARY_PREFIX}${_lib}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    else ()
        set(_libfullname "${CMAKE_STATIC_LIBRARY_PREFIX}${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif ()

    set(_libpath "${PROJECT_BINARY_DIR}/external/lib/${_libfullname}")
    set(_includepath "${PROJECT_BINARY_DIR}/external/include")
    message(STATUS "Configuring ${_target} with ${_libpath}")
    set_property(TARGET ${_target} APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${_libpath}")
    # Manually create the directory, it will be created as part of the build,
    # but this runs in the configuration phase, and CMake generates an error if
    # we add an include directory that does not exist yet.
    file(MAKE_DIRECTORY "${_includepath}")
    set_property(TARGET ${_target} APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${_includepath}")
endfunction ()

function (set_executable_name_for_external_project VAR _exe)
    set(${VAR} "${PROJECT_BINARY_DIR}/external/bin/${_exe}${CMAKE_EXECUTABLE_SUFFIX}" PARENT_SCOPE)
endfunction ()
