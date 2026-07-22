# Copyright 2026 Google LLC
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

if (NOT BUILD_TESTING)
    return()
endif ()

add_library(otel_collector otel_collector.cc otel_collector.h)
target_link_libraries(
    otel_collector PUBLIC google_cloud_cpp_opentelemetry_testing_protos
                          google-cloud-cpp::monitoring_protos gRPC::grpc++)
target_include_directories(otel_collector PUBLIC ${PROJECT_SOURCE_DIR})
google_cloud_cpp_add_common_options(otel_collector NO_WARNINGS)

google_cloud_cpp_add_executable(target "otel_collector_main"
                                "otel_collector_main.cc")
target_link_libraries(otel_collector_main PRIVATE otel_collector gRPC::grpc++)
google_cloud_cpp_add_common_options(otel_collector_main NO_WARNINGS)
