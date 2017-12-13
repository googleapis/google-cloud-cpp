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
#include <absl/base/thread_annotations.h>

namespace {
/// An implementation of the bigtable::AdminClient interface.
class SimpleAdminClient : public bigtable::AdminClient {
 public:
  SimpleAdminClient(std::string project, bigtable::ClientOptions options)
      : project_(std::move(project)), options_(std::move(options)) {}

  std::string const& project() const override { return project_; }
  void on_completion(grpc::Status const& status) override;
  ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&
  table_admin() override;

 private:
  void refresh_credentials_and_channel();

 private:
  std::string project_;
  bigtable::ClientOptions options_;
  mutable std::mutex mu_;
  std::shared_ptr<grpc::Channel> channel_ GUARDED_BY(mu_);
  std::unique_ptr<
      ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
      table_admin_stub_ GUARDED_BY(mu_);
};
}  // anonymous namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::shared_ptr<AdminClient> CreateAdminClient(
    std::string project, bigtable::ClientOptions options) {
  return std::make_shared<SimpleAdminClient>(std::move(project),
                                             std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

namespace {
void SimpleAdminClient::on_completion(grpc::Status const& status) {
  if (status.ok()) {
    return;
  }
  std::unique_lock<std::mutex> lk(mu_);
  channel_.reset();
  table_admin_stub_.reset();
}

::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&
SimpleAdminClient::table_admin() {
  refresh_credentials_and_channel();
  // TODO() - this is inherently unsafe, returning an object that is supposed
  // to be locked.  May need to rethink the interface completely, or declare the
  // class to be not thread-safe.
  return *table_admin_stub_;
}

void SimpleAdminClient::refresh_credentials_and_channel() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (table_admin_stub_) {
      return;
    }
    // Release the lock before executing potentially slow operations.
  }
  auto channel = grpc::CreateCustomChannel(options_.admin_endpoint(),
                                           options_.credentials(),
                                           options_.channel_arguments());
  auto stub =
      ::google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel);
  {
    // Re-acquire the lock before modifying the objects.  There is a small
    // chance that we waste cycles creating two channels and stubs, only to
    // discard one.
    std::lock_guard<std::mutex> lk(mu_);
    table_admin_stub_ = std::move(stub);
    channel_ = std::move(channel);
  }
}

}  // anonymous namespace
