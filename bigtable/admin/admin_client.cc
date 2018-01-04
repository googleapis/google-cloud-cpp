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
/**
 * An AdminClient for single-threaded programs that refreshes credentials on all
 * gRPC errors.
 *
 * This class should not be used by multiple threads, it makes no attempt to
 * protect its critical sections.  While it is rare that the admin interface
 * will be used by multiple threads, we should use the same approach here and in
 * the regular client to support multi-threaded programs.
 *
 * The class also aggressively reconnects on any gRPC errors. A future version
 * should only reconnect on those errors that indicate the credentials or
 * connections need refreshing.
 */
class SimpleAdminClient : public bigtable::AdminClient {
 public:
  SimpleAdminClient(std::string project, bigtable::ClientOptions options)
      : project_(std::move(project)), options_(std::move(options)) {}

  std::string const& project() const override { return project_; }
  void on_completion(grpc::Status const& status) override {
    if (not status.ok()) {
      channel_.reset();
      table_admin_stub_.reset();
    }
  }

  using AdminStubPtr = std::shared_ptr<
      ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>;

  AdminStubPtr Stub() override {
    RefreshCredentialsAndChannel();
    return table_admin_stub_;
  }

 private:
  void RefreshCredentialsAndChannel() {
    if (table_admin_stub_) {
      return;
    }
    auto channel = grpc::CreateCustomChannel(options_.admin_endpoint(),
                                             options_.credentials(),
                                             options_.channel_arguments());
    table_admin_stub_ =
        ::google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel);
    channel_ = std::move(channel);
  }

 private:
  std::string project_;
  bigtable::ClientOptions options_;
  std::shared_ptr<grpc::Channel> channel_;
  AdminStubPtr table_admin_stub_;
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
