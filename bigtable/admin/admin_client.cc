// Copyright 2017 Google Inc.
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

#include "bigtable/admin/admin_client.h"

#include <absl/memory/memory.h>

namespace {
/// An implementation of the bigtable::AdminClient interface.
class SimpleAdminClient : public bigtable::AdminClient {
 public:
  SimpleAdminClient(std::string project, bigtable::ClientOptions options)
      : project_(std::move(project)),
        options_(std::move(options)),
        channel_(),
        table_admin_stub_() {}

  std::string const& project() const override { return project_; }
  void OnFailure(grpc::Status const& status) override;
  ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&
  table_admin() override;

 private:
  void Refresh();

 private:
  std::string project_;
  bigtable::ClientOptions options_;
  mutable std::mutex mu_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<
      google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
      table_admin_stub_;
};
}  // anonymous namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::shared_ptr<AdminClient> CreateAdminClient(
    std::string project, bigtable::ClientOptions options) {
  return std::make_shared<SimpleAdminClient>(std::move(project),
                                             std::move(options));
}

class TableAdmin {
 public:
  TableAdmin(AdminClient& cl, std::string name)
      : client(cl), instance_name(std::move(name)) {}

  AdminClient& client;
  std::string instance_name;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

namespace {
void SimpleAdminClient::OnFailure(grpc::Status const& status) {
  std::unique_lock<std::mutex> lk(mu_);
  channel_.reset();
  table_admin_stub_.reset();
}

::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&
SimpleAdminClient::table_admin() {
  Refresh();
  return *table_admin_stub_;
}

void SimpleAdminClient::Refresh() {
  std::unique_lock<std::mutex> lk(mu_);
  if (table_admin_stub_) {
    return;
  }
  // Release the lock before executing potentially slow operations.
  lk.unlock();
  auto channel = grpc::CreateCustomChannel(options_.admin_endpoint(),
                                           options_.credentials(),
                                           options_.channel_arguments());
  auto stub =
      ::google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel);
  lk.lock();
  table_admin_stub_ = std::move(stub);
  channel_ = std::move(channel);
}

}  // anonymous namespace