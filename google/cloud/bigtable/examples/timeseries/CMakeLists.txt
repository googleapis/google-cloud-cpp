# ~~~
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
# ~~~

# Create a library with the protos for the examples.
add_custom_command(
    OUTPUT "taq.pb.h" "taq.pb.cc"
    COMMAND protoc
            ARGS
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}
                                                  taq.proto
    DEPENDS taq.proto protoc
    COMMENT "Running (local) C++ protocol buffer compiler on ${FIL}"
    VERBATIM)
add_library(taq_protos "taq.pb.h" "taq.pb.cc")
target_link_libraries(taq_protos gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
target_include_directories(taq_protos PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

set(timeseries_demos
    compute_spread
    compute_vwap
    estimate_split_points
    upload_taq_nbbo
    upload_taq_trades
    shuffle_taq_nbbo
    shuffle_taq_trades)

foreach (demo IN LISTS timeseries_demos)
    add_executable("${demo}" "${demo}.cc" "circular_buffer.h")
    target_link_libraries(${demo}
                          taq_protos
                          bigtable_client
                          bigtable::protos
                          gRPC::grpc++
                          gRPC::grpc
                          protobuf::libprotobuf)
endforeach ()
