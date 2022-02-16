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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H

#include "google/cloud/bigtable/admin/bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates a `bigtable_admin::BigtableTableAdminConnection` and a
 * `CompletionQueue` for `bigtable::TableAdmin` to use.
 *
 * This class is used to initiate a connection to the Cloud Bigtable Table
 * Admin service. It is maintained only for backwards compatibility.
 *
 * @note Please prefer using `bigtable_admin::BigtableTableAdminConnection` to
 *     configure `bigtable_admin::BigtableTableAdminClient`, instead of using
 *     this class to configure `bigtable::TableAdmin`.
 */
class AdminClient {
 public:
  virtual ~AdminClient() = default;

  /// The project id that this AdminClient works on.
  virtual std::string const& project() const = 0;

 private:
  friend class TableAdmin;

  virtual std::shared_ptr<bigtable_admin::BigtableTableAdminConnection>
  connection() = 0;

  virtual CompletionQueue cq() = 0;

  virtual std::shared_ptr<BackgroundThreads> background_threads() = 0;
};

/// Create a new table admin client configured via @p options.
std::shared_ptr<AdminClient> MakeAdminClient(std::string project,
                                             Options options = {});

/**
 * Create a new table admin client configured via @p options.
 *
 * @deprecated use the `MakeAdminClient` method which accepts
 * `google::cloud::Options` instead.
 */
std::shared_ptr<AdminClient> CreateDefaultAdminClient(std::string project,
                                                      ClientOptions options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H
