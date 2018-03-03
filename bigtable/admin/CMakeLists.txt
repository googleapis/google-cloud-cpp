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

add_library(bigtable_admin_client
        admin_client.h
        admin_client.cc
        column_family.h
        instance_admin_client.h
        instance_admin_client.cc
        instance_admin.h
        instance_admin.cc
        table_admin.h
        table_admin.cc
        table_config.h
        table_config.cc)
target_link_libraries(bigtable_admin_client bigtable_client bigtable_protos
        gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
target_include_directories(bigtable_admin_client PUBLIC "${PROJECT_SOURCE_DIR}")
add_library(bigtable::admin_client ALIAS bigtable_admin_client)

# List the unit tests, then setup the targets and dependencies.
set(bigtable_admin_unit_tests
        admin_client_test.cc
        column_family_test.cc
        instance_admin_client_test.cc
        instance_admin_test.cc
        table_admin_test.cc
        table_config_test.cc)
foreach (fname ${bigtable_admin_unit_tests})
    string(REPLACE "/" "_" target ${fname})
    string(REPLACE ".cc" "" target ${target})
    set(target "bigtable_admin_${target}")
    add_executable(${target} ${fname})
    target_link_libraries(${target} PRIVATE
            bigtable_admin_client bigtable_client bigtable_protos
            gmock gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
    add_test(NAME ${target} COMMAND ${target})
    add_dependencies(tests-local ${target})
endforeach ()
