# ~~~
# Copyright 2024 Google LLC
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

add_library(
    bigtable_emulator_common # cmake-format: sort
    cell_view.h
    cluster.cc
    cluster.h
    column_family.cc
    column_family.h
    column_family_test.cc
    filter.cc
    filter.h
    row_iterators.cc
    row_iterators.h
    row_streamer.cc
    row_streamer.h
    server.cc
    server.h
    sorted_row_set.cc
    sorted_row_set.h
    table.cc
    table.h
    to_grpc_status.cc
    to_grpc_status.h)


target_link_libraries(
    bigtable_emulator_common
    google-cloud-cpp::bigtable
    google-cloud-cpp::bigtable_protos
    google-cloud-cpp::common
    google-cloud-cpp::grpc_utils
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf)
google_cloud_cpp_add_common_options(bigtable_emulator_common)

include(CreateBazelConfig)
create_bazel_config(bigtable_emulator_common YEAR 2024)

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(bigtable_emulator_unit_tests
        # cmake-format: sort
        column_family_test.cc
        filter_test.cc
        row_iterators_test.cc
        server_test.cc
        sorted_row_set_test.cc)
    export_list_to_bazel("bigtable_emulator_unit_tests.bzl"
                         "bigtable_emulator_unit_tests" YEAR "2024")

    foreach (fname ${bigtable_emulator_unit_tests})
        google_cloud_cpp_add_executable(target "bigtable_emulator" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigtable_emulator_common
                    bigtable_client_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::bigtable
                    google-cloud-cpp::bigtable_protos
                    google-cloud-cpp::common
                    google-cloud-cpp::grpc_utils
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    gRPC::grpc++
                    gRPC::grpc
                    protobuf::libprotobuf)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endif ()

set(bigtable_emulator_programs
    # cmake-format: sort
    emulator.cc)
export_list_to_bazel("bigtable_emulator_programs.bzl"
                     "bigtable_emulator_programs" YEAR "2024")

foreach (fname ${bigtable_emulator_programs})
    google_cloud_cpp_add_executable(target "bigtable" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE bigtable_emulator_common
                google-cloud-cpp::bigtable
                google-cloud-cpp::bigtable_protos
                google-cloud-cpp::grpc_utils
                google_cloud_cpp_testing
                gRPC::grpc++
                gRPC::grpc
                protobuf::libprotobuf)
    google_cloud_cpp_add_common_options(${target})
    if (BUILD_TESTING)
        add_test(NAME ${target} COMMAND ${target})
    endif ()
endforeach ()
