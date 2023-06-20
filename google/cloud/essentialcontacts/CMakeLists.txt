# ~~~
# Copyright 2023 Google LLC
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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Essential Contacts API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the Essential Contacts API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

unset(mocks_globs)
unset(source_globs)
set(service_dirs "v1/")
foreach (dir IN LISTS service_dirs)
    string(REPLACE "/" "_" ns "${dir}")
    list(APPEND source_globs "${dir}*.h" "${dir}*.cc" "${dir}internal/*")
    list(APPEND mocks_globs "${dir}mocks/*.h")
    list(APPEND DOXYGEN_EXAMPLE_PATH
         "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
    list(APPEND DOXYGEN_EXCLUDE_SYMBOLS "essentialcontacts_${ns}internal")
endforeach ()

include(GoogleCloudCppDoxygen)
google_cloud_cpp_doxygen_targets("essentialcontacts" DEPENDS cloud-docs
                                 google-cloud-cpp::essentialcontacts_protos)

include(GoogleCloudCppCommon)

include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/essentialcontacts.list"
)
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/essentialcontacts.deps"
)
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_essentialcontacts_protos # cmake-format: sort
    ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(essentialcontacts_protos)
target_link_libraries(google_cloud_cpp_essentialcontacts_protos
                      PUBLIC ${proto_deps})

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${source_globs})
list(SORT source_files)
add_library(google_cloud_cpp_essentialcontacts ${source_files})
target_include_directories(
    google_cloud_cpp_essentialcontacts
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_essentialcontacts
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::essentialcontacts_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_essentialcontacts)
set_target_properties(
    google_cloud_cpp_essentialcontacts
    PROPERTIES EXPORT_NAME google-cloud-cpp::essentialcontacts
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_essentialcontacts
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::essentialcontacts ALIAS
            google_cloud_cpp_essentialcontacts)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
file(
    GLOB relative_mock_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${mocks_globs})
list(SORT relative_mock_files)
set(mock_files)
foreach (file IN LISTS relative_mock_files)
    list(APPEND mock_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach ()
add_library(google_cloud_cpp_essentialcontacts_mocks INTERFACE)
target_sources(google_cloud_cpp_essentialcontacts_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_essentialcontacts_mocks
    INTERFACE google-cloud-cpp::essentialcontacts GTest::gmock_main
              GTest::gmock GTest::gtest)
set_target_properties(
    google_cloud_cpp_essentialcontacts_mocks
    PROPERTIES EXPORT_NAME google-cloud-cpp::essentialcontacts_mocks)
target_include_directories(
    google_cloud_cpp_essentialcontacts_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_essentialcontacts_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CTest)
if (BUILD_TESTING)
    add_executable(essentialcontacts_quickstart "quickstart/quickstart.cc")
    target_link_libraries(essentialcontacts_quickstart
                          PRIVATE google-cloud-cpp::essentialcontacts)
    google_cloud_cpp_add_common_options(essentialcontacts_quickstart)
    add_test(
        NAME essentialcontacts_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:essentialcontacts_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(
        essentialcontacts_quickstart PROPERTIES DISABLED TRUE LABELS
                                                "integration-test;quickstart")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_essentialcontacts-targets
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_essentialcontacts"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_essentialcontacts
            google_cloud_cpp_essentialcontacts_protos
    EXPORT google_cloud_cpp_essentialcontacts-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_essentialcontacts
            google_cloud_cpp_essentialcontacts_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_essentialcontacts_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_essentialcontacts_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_essentialcontacts"
                                 "include/google/cloud/essentialcontacts")
google_cloud_cpp_install_headers("google_cloud_cpp_essentialcontacts_mocks"
                                 "include/google/cloud/essentialcontacts")

google_cloud_cpp_add_pkgconfig(
    essentialcontacts
    "The Essential Contacts API C++ Client Library"
    "Provides C++ APIs to use the Essential Contacts API."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_essentialcontacts_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in"
               "google_cloud_cpp_essentialcontacts-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_essentialcontacts-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_essentialcontacts-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_essentialcontacts-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_essentialcontacts"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_essentialcontacts_protos")

# google-cloud-cpp::essentialcontacts must be defined before we can add the
# samples.
foreach (dir IN LISTS service_dirs)
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        google_cloud_cpp_add_samples_relative("essentialcontacts"
                                              "${dir}samples/")
    endif ()
endforeach ()
