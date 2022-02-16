// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/admin_client_params.h"
#include "google/cloud/bigtable/internal/defaults.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

class DefaultAdminClient : public google::cloud::bigtable::AdminClient {
 public:
  DefaultAdminClient(std::string project,
                     bigtable_internal::AdminClientParams params)
      : project_(std::move(project)),
        cq_(params.options.get<GrpcCompletionQueueOption>()),
        background_threads_(std::move(params.background_threads)),
        connection_(bigtable_admin::MakeBigtableTableAdminConnection(
            std::move(params.options))) {}

  std::string const& project() const override { return project_; }

 private:
  std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> connection()
      override {
    return connection_;
  }

  CompletionQueue cq() override { return cq_; }

  std::shared_ptr<BackgroundThreads> background_threads() override {
    return background_threads_;
  }

  std::string project_;
  CompletionQueue cq_;
  std::shared_ptr<BackgroundThreads> background_threads_;
  std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> connection_;
};

}  // namespace

std::shared_ptr<AdminClient> MakeAdminClient(std::string project,
                                             Options options) {
  auto params = bigtable_internal::AdminClientParams(
      internal::DefaultTableAdminOptions(std::move(options)));
  return std::make_shared<DefaultAdminClient>(std::move(project),
                                              std::move(params));
}

std::shared_ptr<AdminClient> CreateDefaultAdminClient(std::string project,
                                                      ClientOptions options) {
  return MakeAdminClient(std::move(project),
                         internal::MakeOptions(std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
