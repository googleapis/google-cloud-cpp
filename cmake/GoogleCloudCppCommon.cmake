# ~~~
# Copyright 2018 Google LLC
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

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Helper functions to set common options, e.g., enabling -Werror
include(GoogleCloudCppCommonOptions)

# Helper functions to create pkg-config(1) module files.
include(AddPkgConfig)

#
# google_cloud_cpp_install_headers : install all the headers in a target
#
# Find all the headers in @p target and install them at @p destination,
# preserving the directory structure.
#
# * target the name of the target.
# * destination the destination directory, relative to <PREFIX>. Typically this
#   starts with `include/google/cloud`, the function requires the full
#   destination in case some headers get installed elsewhere in the future.
#
function (google_cloud_cpp_install_headers target destination)
    get_target_property(target_type ${target} TYPE)
    if ("${target_type}" STREQUAL "INTERFACE_LIBRARY")
        # For interface libraries we use `INTERFACE_SOURCES` to get the list of
        # sources, which are actually just headers in this case.
        get_target_property(target_sources ${target} INTERFACE_SOURCES)
    else ()
        get_target_property(target_sources ${target} SOURCES)
    endif ()
    foreach (header ${target_sources})
        # Sometimes we use generator expressions for the headers.
        string(REPLACE "$<BUILD_INTERFACE:" "" header "${header}")
        string(REPLACE ">" "" header "${header}")

        # Only install headers.
        if (NOT "${header}" MATCHES "\\.h$" AND NOT "${header}" MATCHES
                                                "\\.inc$")
            continue()
        endif ()
        # Sometimes we generate header files into the binary directory, do not
        # forget to install those with their relative path.
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" relative "${header}")
        # INTERFACE libraries use absolute paths, yuck.
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" relative "${relative}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(
            FILES "${header}"
            DESTINATION "${destination}/${dir}"
            COMPONENT google_cloud_cpp_development)
    endforeach ()
endfunction ()

#
# google_cloud_cpp_set_target_name : formats the prefix and path as a target
#
# The formatted target name looks like "<prefix>_<basename>" where <basename> is
# computed from the path. A 4th argument may optionally be specified, which
# should be the name of a variable in the parent's scope where the <basename>
# should be set. This is useful only if the caller wants both the target name
# and the basename.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the product, such as "pubsub" or "storage".
# * path is the filename that should be used for the target.
# * (optional) basename the name of a variable to be set in the parent scope
#   containing the basename of the target.
#
function (google_cloud_cpp_set_target_name target prefix path)
    string(REPLACE "/" "_" basename "${path}")
    string(REPLACE ".cc" "" basename "${basename}")
    set("${target}"
        "${prefix}_${basename}"
        PARENT_SCOPE)
    # Optional 4th argument, will be set to the basename if present.
    if (ARGC EQUAL 4)
        set("${ARGV3}"
            "${basename}"
            PARENT_SCOPE)
    endif ()
endfunction ()

#
# google_cloud_cpp_add_executable : adds an executable w/ the given source and
# prefix name
#
# Computes the target name using `google_cloud_cpp_set_target_name` (see above),
# then adds an executable with a few common properties. Sets the `target` in the
# caller's scope to the name of the computed target name.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the product, such as "pubsub" or "storage".
# * path is the filename that should be used for the target.
#
function (google_cloud_cpp_add_executable target prefix source)
    google_cloud_cpp_set_target_name(target_name "${prefix}" "${source}"
                                     shortname)
    add_executable("${target_name}" "${source}")
    set_target_properties("${target_name}" PROPERTIES OUTPUT_NAME ${shortname})
    set("${target}"
        "${target_name}"
        PARENT_SCOPE)
endfunction ()

# google_cloud_cpp_add_samples_relative : adds rules to compile and test
# generated samples for $library found in the relative $path.
function (google_cloud_cpp_add_samples_relative library path)
    file(
        GLOB sample_files
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${path}*.cc")
    foreach (source IN LISTS sample_files)
        google_cloud_cpp_add_executable(target "${library}" "${source}")
        if (TARGET google-cloud-cpp::${library})
            target_link_libraries(
                "${target}" PRIVATE google-cloud-cpp::${library}
                                    google_cloud_cpp_testing)
        elseif (TARGET google-cloud-cpp::experimental-${library})
            target_link_libraries(
                "${target}" PRIVATE google-cloud-cpp::experimental-${library}
                                    google_cloud_cpp_testing)
        endif ()
        google_cloud_cpp_add_common_options("${target}")
        add_test(NAME "${target}" COMMAND "${target}")
        set_tests_properties("${target}" PROPERTIES LABELS "integration-test")
    endforeach ()
endfunction ()

# google_cloud_cpp_add_samples : adds rules to compile and test generated
# samples
function (google_cloud_cpp_add_samples library)
    google_cloud_cpp_add_samples_relative("${library}" "")
endfunction ()

# Install headers, export target, and add a package config file for a `*_mocks`
# library.
#
# Parameters:
#
# * library:      e.g. pubsub
# * display_name: e.g. "Cloud Pub/Sub"
function (google_cloud_cpp_install_mocks library display_name)
    set(library_target "google_cloud_cpp_${library}")
    set(mocks_target "google_cloud_cpp_${library}_mocks")
    install(
        EXPORT ${mocks_target}-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${mocks_target}"
        COMPONENT google_cloud_cpp_development)
    install(
        TARGETS ${mocks_target}
        EXPORT ${mocks_target}-targets
        COMPONENT google_cloud_cpp_development)

    google_cloud_cpp_install_headers("${mocks_target}"
                                     "include/google/cloud/${library}")

    google_cloud_cpp_add_pkgconfig(
        ${library}_mocks "${display_name} Mocks"
        "Mocks for the ${display_name} C++ Client Library" "${library_target}"
        "gmock")

    set(GOOGLE_CLOUD_CPP_CONFIG_LIBRARY "${library_target}")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/mocks-config.cmake.in"
                   "${mocks_target}-config.cmake" @ONLY)
    write_basic_package_version_file(
        "${mocks_target}-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion)

    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/${mocks_target}-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/${mocks_target}-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${mocks_target}"
        COMPONENT google_cloud_cpp_development)
endfunction ()
