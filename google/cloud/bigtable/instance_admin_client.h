// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H

#include "google/cloud/bigtable/admin/bigtable_instance_admin_connection.h"
#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates a `bigtable_admin::BigtableInstanceAdminConnection` for
 * `bigtable::InstanceAdmin` to use.
 *
 * This class is used to initiate a connection to the Cloud Bigtable Instance
 * Admin service. It is maintained only for backwards compatibility.
 *
 * @deprecated Please use `bigtable_admin::BigtableInstanceAdminConnection` to
 *     configure `bigtable_admin::BigtableInstanceAdminClient`, instead of using
 *     this class to configure `bigtable::InstanceAdmin`.
 */
class InstanceAdminClient final {
 public:
  virtual ~InstanceAdminClient() = default;

  /// The project id that this AdminClient works on.
  virtual std::string const& project() { return project_; }

 private:
  friend class InstanceAdmin;
  friend std::shared_ptr<InstanceAdminClient> MakeInstanceAdminClient(
      std::string, Options);

  InstanceAdminClient(std::string project, Options options)
      : project_(std::move(project)),
        connection_(bigtable_admin::MakeBigtableInstanceAdminConnection(
            std::move(options))) {}

  std::string project_;
  std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection> connection_;
};

/// Create a new instance admin client configured via @p options.
std::shared_ptr<InstanceAdminClient> MakeInstanceAdminClient(
    std::string project, Options options = {});

/**
 * Create a new instance admin client configured via @p options.
 *
 * @deprecated use the `MakeInstanceAdminClient` method which accepts
 * `google::cloud::Options` instead.
 */
GOOGLE_CLOUD_CPP_DEPRECATED("use `MakeInstanceAdminClient` instead")
std::shared_ptr<InstanceAdminClient> CreateDefaultInstanceAdminClient(
    std::string project, ClientOptions options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H
