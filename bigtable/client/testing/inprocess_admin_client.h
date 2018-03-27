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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_ADMIN_CLIENT_H_

#include "bigtable/client/admin_client.h"
#include "bigtable/client/client_options.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>

namespace btadmin = ::google::bigtable::admin::v2;

namespace bigtable {
namespace testing {

/**
 * Connects to Cloud Bigtable's administration APIs.
 *
 * This class is mainly for testing purpose, it enable use of a single embedded
 * server
 * for multiple test cases. This adminlient uses a pre-defined channel.
 */
class InProcessAdminClient : public bigtable::AdminClient {
 public:
  InProcessAdminClient(std::string project,
                       std::shared_ptr<grpc::Channel> channel)
      : project_(std::move(project)), channel_(std::move(channel)) {}

  using BigtableAdminStubPtr =
      std::shared_ptr<btadmin::BigtableTableAdmin::StubInterface>;

  std::string const& project() const override { return project_; }
  BigtableAdminStubPtr Stub() override {
    return btadmin::BigtableTableAdmin::NewStub(channel_);
  }
  void reset() override {}
  void on_completion(grpc::Status const& status) override {}

 private:
  std::string project_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_ADMIN_CLIENT_H_
