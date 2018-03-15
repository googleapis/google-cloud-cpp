// Copyright 2018 Google Inc.
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

#include "bigtable/client/instance_admin_client.h"
#include "bigtable/client/internal/common_client.h"

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
class DefaultInstanceAdminClient : public bigtable::InstanceAdminClient {
 private:
  // Introduce an early `private:` section because this type is used to define
  // the public interface, it should not be part of the public interface.
  struct AdminTraits {
    static std::string const& Endpoint(bigtable::ClientOptions& options) {
      return options.admin_endpoint();
    }
  };

  using Impl = bigtable::internal::CommonClient<
      AdminTraits, ::google::bigtable::admin::v2::BigtableInstanceAdmin>;

 public:
  using AdminStubPtr = Impl::StubPtr;

  DefaultInstanceAdminClient(std::string project,
                             bigtable::ClientOptions options)
      : project_(std::move(project)), impl_(std::move(options)) {}

  std::string const& project() const override { return project_; }
  AdminStubPtr Stub() override { return impl_.Stub(); }
  void reset() override { return impl_.reset(); }
  void on_completion(grpc::Status const& status) override {}

  DefaultInstanceAdminClient(DefaultInstanceAdminClient const&) = delete;
  DefaultInstanceAdminClient& operator=(DefaultInstanceAdminClient const&) =
      delete;

 private:
  std::string project_;
  Impl impl_;
};
}  // anonymous namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::shared_ptr<InstanceAdminClient> CreateDefaultInstanceAdminClient(
    std::string project, bigtable::ClientOptions options) {
  return std::make_shared<DefaultInstanceAdminClient>(std::move(project),
                                                      std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
