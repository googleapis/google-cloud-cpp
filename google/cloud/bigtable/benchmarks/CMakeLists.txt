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

add_library(bigtable_benchmark_common
            benchmark.h
            benchmark.cc
            constants.h
            embedded_server.h
            embedded_server.cc
            random_mutation.h
            random_mutation.cc
            setup.h
            setup.cc)
target_link_libraries(bigtable_benchmark_common
                      bigtable_client_testing
                      bigtable_client
                      bigtable_protos
                      gRPC::grpc++
                      gRPC::grpc
                      protobuf::libprotobuf)

# List the unit tests, then setup the targets and dependencies.
set(bigtable_benchmarks_unit_tests
    bigtable_benchmark_test.cc
    embedded_server_test.cc
    format_duration_test.cc
    setup_test.cc)
foreach (fname ${bigtable_benchmarks_unit_tests})
    string(REPLACE "/"
                   "_"
                   target
                   ${fname})
    string(REPLACE ".cc"
                   ""
                   target
                   ${target})
    add_executable(${target} ${fname})
    target_link_libraries(${target}
                          PRIVATE bigtable_benchmark_common
                                  bigtable_client
                                  bigtable_protos
                                  bigtable_common_options
                                  google_cloud_cpp_testing
                                  google_cloud_cpp_common
                                  gmock
                                  gRPC::grpc++
                                  gRPC::grpc
                                  protobuf::libprotobuf)
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Benchmark Table::ReadRows().
add_executable(scan_throughput_benchmark scan_throughput_benchmark.cc)
target_link_libraries(scan_throughput_benchmark
                      PRIVATE bigtable_benchmark_common
                              bigtable_client
                              bigtable_protos
                              bigtable_common_options
                              gRPC::grpc++
                              gRPC::grpc
                              protobuf::libprotobuf)

# Benchmark for Table::Apply() and Table::ReadRow().
add_executable(apply_read_latency_benchmark apply_read_latency_benchmark.cc)
target_link_libraries(apply_read_latency_benchmark
                      PRIVATE bigtable_benchmark_common
                              bigtable_client
                              bigtable_protos
                              bigtable_common_options
                              gRPC::grpc++
                              gRPC::grpc
                              protobuf::libprotobuf)

# A benchmark to measure performance of long running programs.
add_executable(endurance_benchmark endurance_benchmark.cc)
target_link_libraries(endurance_benchmark
                      PRIVATE bigtable_benchmark_common
                              bigtable_client
                              bigtable_protos
                              bigtable_common_options
                              gRPC::grpc++
                              gRPC::grpc
                              protobuf::libprotobuf)
