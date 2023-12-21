# ~~~
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# We do not use macros a lot, so this deserves a comment. Unlike functions,
# macros do not introduce a scope. This is an advantage when trying to set
# global variables, as we do here.  It is obviously a disadvantage if you need
# local variables.
macro (google_cloud_cpp_set_pkgconfig_paths)
    if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(GOOGLE_CLOUD_CPP_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else ()
        set(GOOGLE_CLOUD_CPP_PC_LIBDIR
            "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif ()

    if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(GOOGLE_CLOUD_CPP_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else ()
        set(GOOGLE_CLOUD_CPP_PC_INCLUDEDIR
            "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif ()
endmacro ()

#
# Create the pkgconfig configuration file (aka *.pc file) and the rules to
# install it.
#
# * library: the name of the library, such as `storage`, or `spanner`
# * ARGN: the names of any pkgconfig modules the generated module depends on
#
function (google_cloud_cpp_add_pkgconfig library name description)
    cmake_parse_arguments(_opt "WITH_SHORT_TARGET" "" "" ${ARGN})
    if (_opt_WITH_SHORT_TARGET)
        set(target "${library}")
    else ()
        set(target "google_cloud_cpp_${library}")
    endif ()
    set(GOOGLE_CLOUD_CPP_PC_NAME "${name}")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "${description}")
    string(JOIN " " GOOGLE_CLOUD_CPP_PC_REQUIRES ${_opt_UNPARSED_ARGUMENTS})
    google_cloud_cpp_set_pkgconfig_paths()
    get_target_property(target_type ${target} TYPE)
    if ("${target_type}" STREQUAL "INTERFACE_LIBRARY")
        # Interface libraries only contain headers. They do not generate lib
        # files to link against with `-l`.
        set(GOOGLE_CLOUD_CPP_PC_LIBS "")
    else ()
        set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_${library}")
    endif ()
    get_target_property(target_defs ${target} INTERFACE_COMPILE_DEFINITIONS)
    if (target_defs)
        foreach (def ${target_defs})
            string(APPEND GOOGLE_CLOUD_CPP_PC_CFLAGS " -D${def}")
        endforeach ()
    endif ()

    # Create and install the pkg-config files.
    configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.pc.in"
                   "google_cloud_cpp_${library}.pc" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_${library}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT google_cloud_cpp_development)
endfunction ()
