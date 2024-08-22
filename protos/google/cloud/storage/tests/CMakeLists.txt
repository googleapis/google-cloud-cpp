# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

# Signed URL conformance tests are parsed using protos. In order to simplify the
# build files, only build these conformance tests if gRPC in GCS is compiled.
if ((NOT GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC) OR (NOT BUILD_TESTING))
    return()
endif ()

include(GoogleCloudCppCommon)
find_package(Protobuf CONFIG QUIET)
if (NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif ()

# Generate protobuf code and library
if (COMMAND protobuf_generate)
    set(protobuf_PROTOC_EXE ${Protobuf_PROTOC_EXECUTABLE})
    add_library(google_cloud_cpp_storage_tests_conformance_protos)
    protobuf_generate(
        LANGUAGE cpp TARGET google_cloud_cpp_storage_tests_conformance_protos
        PROTOS conformance_tests.proto)
elseif (COMMAND protobuf_generate_cpp)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS conformance_tests.proto)
    add_library(google_cloud_cpp_storage_tests_conformance_protos ${PROTO_HDRS}
                                                                  ${PROTO_SRCS})
else ()
    message(FATAL_ERROR "Missing protobuf_generate_cpp and protobuf_generate")
endif ()

target_link_libraries(google_cloud_cpp_storage_tests_conformance_protos
                      PUBLIC protobuf::libprotobuf)
target_include_directories(google_cloud_cpp_storage_tests_conformance_protos
                           SYSTEM PUBLIC "${PROJECT_BINARY_DIR}/protos")
google_cloud_cpp_add_common_options(
    google_cloud_cpp_storage_tests_conformance_protos NO_WARNINGS)
set_target_properties(google_cloud_cpp_storage_tests_conformance_protos
                      PROPERTIES CXX_CLANG_TIDY "")
