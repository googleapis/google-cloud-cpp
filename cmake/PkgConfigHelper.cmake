# ~~~
# Copyright 2018 Google Inc.
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

# Convert the macros created by pkg_check_modules() into properties of an
# imported library target.
#
# We want to be able to use targets such as gRPC::grpc++ to represent an
# external library.  That is supported by CMake via IMPORTED libraries, but one
# has to set the properties of the library explicitly, such as the full path to
# the library, and the include directories.
#
# We can use pkg_check_modules() to find the right command-line flags to compile
# and link against such libraries.  This function converts the flags discovered
# by pkg_check_modules(), such as "-L/foo/bar -lbaz" into the right properties
# for the given target, such as /foo/bar/libbaz.so added to the
# INTERFACE_LINK_LIBRARIES property.
function (set_library_properties_from_pkg_config _target _prefix)

    # We need to search for the libraries because the properties require
    # absolute paths.
    unset(_libs)
    unset(_find_opts)
    unset(_search_paths)

    # Use find_library() to find each one of the -l options, and add the results
    # to the _libs variable.
    foreach (flag IN LISTS ${_prefix}_LDFLAGS)

        # only look into the given paths from now on
        if (flag MATCHES "^-L(.*)")
            list(APPEND _search_paths ${CMAKE_MATCH_1})
            set(_find_opts HINTS ${_search_paths} NO_DEFAULT_PATH)
            continue()
        endif ()
        if (flag MATCHES "^-l(.*)")
            set(_pkg_search "${CMAKE_MATCH_1}")
        else()
            continue()
        endif ()

        find_library(pkgcfg_lib_${_prefix}_${_pkg_search}
                     NAMES ${_pkg_search} ${_find_opts})

        # Sometimes the library is not found but it is not a problem, for
        # example, protobuf links -lpthread, but that is not found here because
        # we explicitly exclude the standard search paths.
        if (pkgcfg_lib_${_prefix}_${_pkg_search})
            list(APPEND _libs "${pkgcfg_lib_${_prefix}_${_pkg_search}}")
        endif ()
    endforeach ()
    if (_libs)
        set_property(TARGET ${_target}
                     PROPERTY INTERFACE_LINK_LIBRARIES "${_libs}")
    endif ()
    if (${_prefix}_INCLUDE_DIRS)
        set_property(TARGET ${_target}
                     PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                              "${${_prefix}_INCLUDE_DIRS}")
    endif ()
    if (${_prefix}_CFLAGS_OTHER)
        set_property(TARGET ${_target}
                     PROPERTY INTERFACE_COMPILE_OPTIONS
                              "${${_prefix}_CFLAGS_OTHER}")
    endif ()
endfunction ()
