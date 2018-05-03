// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_DATA_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_DATA_CLIENT_H_

#include "bigtable/client/instance_admin_client.h"
#include <gmock/gmock.h>

namespace bigtable {
namespace testing {

class MockInstanceAdminClient : public bigtable::InstanceAdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(Channel, std::shared_ptr<grpc::Channel>());
  MOCK_METHOD0(reset, void());
  MOCK_METHOD3(
      ListInstances,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::ListInstancesRequest const&,
                   google::bigtable::admin::v2::ListInstancesResponse*));
  MOCK_METHOD3(
      CreateInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::CreateInstanceRequest const&,
                   google::longrunning::Operation*));

  MOCK_METHOD3(GetOperation,
               grpc::Status(grpc::ClientContext*,
                            google::longrunning::GetOperationRequest const&,
                            google::longrunning::Operation*));

  MOCK_METHOD3(
      GetInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::GetInstanceRequest const&,
                   google::bigtable::admin::v2::Instance*));

  MOCK_METHOD3(
      DeleteInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::DeleteInstanceRequest const&,
                   google::protobuf::Empty*));
};

}  // namespace testing
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_DATA_CLIENT_H_
