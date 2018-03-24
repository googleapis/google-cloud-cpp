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

# The integration tests, these are simply programs that connect to the
# Cloud Bigtable emulator.
add_executable(data_integration_test data_integration_test.cc)
target_link_libraries(data_integration_test
        gmock bigtable_client bigtable_client_testing bigtable_protos
        gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
add_dependencies(tests-local data_integration_test)

# An integration test for bigtable::TableAdmin.
add_executable(admin_integration_test admin_integration_test.cc)
target_link_libraries(admin_integration_test
        gmock bigtable_client bigtable_client_testing bigtable_protos
        gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
add_dependencies(tests-local admin_integration_test)

# An integration test for bigtable::Filter.
add_executable(filters_integration_test filters_integration_test.cc)
target_link_libraries(filters_integration_test
        gmock bigtable_client bigtable_client_testing bigtable_protos
        gRPC::grpc protobuf::libprotobuf)
add_dependencies(tests-local filters_integration_test)

# An integration test for bigtable::Mutations.
add_executable(mutations_integration_test mutations_integration_test.cc)
target_link_libraries(mutations_integration_test
        gmock bigtable_client bigtable_client_testing bigtable_protos
        gRPC::grpc protobuf::libprotobuf)
add_dependencies(tests-local mutations_integration_test)

# Integration tests for bigtable::InstanceAdmin.
add_executable(instance_admin_integration_test instance_admin_integration_test.cc)
target_link_libraries(instance_admin_integration_test PRIVATE
    gmock bigtable_client bigtable_client_testing bigtable_protos
    gRPC::grpc protobuf::libprotobuf)
add_dependencies(tests-local instance_admin_integration_test)
