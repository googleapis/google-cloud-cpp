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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Cloud Speech-to-Text API C++ Client")
set(DOXYGEN_PROJECT_BRIEF
    "A C++ Client Library for the Cloud Speech-to-Text API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal")
set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")

unset(mocks_globs)
unset(source_globs)
set(service_dirs "" "v2/")
foreach (dir IN LISTS service_dirs)
    string(REPLACE "/" "_" ns "${dir}")
    list(APPEND source_globs "${dir}*.h" "${dir}*.cc" "${dir}internal/*")
    list(APPEND mocks_globs "${dir}mocks/*.h")
    list(APPEND DOXYGEN_EXAMPLE_PATH
         "${CMAKE_CURRENT_SOURCE_DIR}/${dir}samples")
    list(APPEND DOXYGEN_EXCLUDE_SYMBOLS "speech_${ns}internal")
endforeach ()

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::speech_protos)

include(GoogleCloudCppCommon)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/speech.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/speech.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_speech_protos # cmake-format: sort
    ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(speech_protos)
target_link_libraries(google_cloud_cpp_speech_protos PUBLIC ${proto_deps})

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    ${source_globs})
list(SORT source_files)
add_library(google_cloud_cpp_speech ${source_files})
target_include_directories(
    google_cloud_cpp_speech
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_speech
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::speech_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_speech)
set_target_properties(
    google_cloud_cpp_speech
    PROPERTIES EXPORT_NAME google-cloud-cpp::speech
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_speech
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::speech ALIAS google_cloud_cpp_speech)

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
add_library(google_cloud_cpp_speech_mocks INTERFACE)
target_sources(google_cloud_cpp_speech_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_speech_mocks
    INTERFACE google-cloud-cpp::speech GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(google_cloud_cpp_speech_mocks
                      PROPERTIES EXPORT_NAME google-cloud-cpp::speech_mocks)
target_include_directories(
    google_cloud_cpp_speech_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_speech_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CTest)
if (BUILD_TESTING)
    add_executable(speech_quickstart "quickstart/quickstart.cc")
    target_link_libraries(speech_quickstart PRIVATE google-cloud-cpp::speech)
    google_cloud_cpp_add_common_options(speech_quickstart)
    add_test(
        NAME speech_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:speech_quickstart>)
    set_tests_properties(
        speech_quickstart
        PROPERTIES
            LABELS "integration-test;quickstart" ENVIRONMENT
            GOOGLE_CLOUD_CPP_USER_PROJECT=$ENV{GOOGLE_CLOUD_CPP_USER_PROJECT})
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_speech-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_speech"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_speech google_cloud_cpp_speech_protos
    EXPORT google_cloud_cpp_speech-targets
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
    TARGETS google_cloud_cpp_speech google_cloud_cpp_speech_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos("google_cloud_cpp_speech_protos"
                                              "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers("google_cloud_cpp_speech_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_speech"
                                 "include/google/cloud/speech")
google_cloud_cpp_install_headers("google_cloud_cpp_speech_mocks"
                                 "include/google/cloud/speech")

google_cloud_cpp_add_pkgconfig(
    speech
    "The Cloud Speech-to-Text API C++ Client Library"
    "Provides C++ APIs to use the Cloud Speech-to-Text API."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_speech_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_speech-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_speech-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_speech-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_speech-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_speech"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_speech_protos")

# google-cloud-cpp::speech must be defined before we can add the samples.
foreach (dir IN LISTS service_dirs)
    if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
        google_cloud_cpp_add_samples_relative("speech" "${dir}samples/")
    endif ()
endforeach ()
