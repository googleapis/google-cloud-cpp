// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/testing/debug_log.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void LogUpdateDatabaseDdl(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    google::cloud::spanner::Database const& database,
    google::cloud::Status const& status) {
  if (status.ok()) return;

  GCP_LOG(DEBUG) << std::string(26, '=') << " UpdateDatabaseDdl() "
                 << std::string(26, '=');

  // Call GetDatabaseDdl() and ListDatabaseOperations() so that their
  // RPC traces can give us information about the state of the database.
  auto ddl = client.GetDatabaseDdl(database.FullName());
  google::spanner::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent(database.instance().FullName());
  request.set_filter("name:" + database.FullName() + "/");
  for (auto const& operation : client.ListDatabaseOperations(request)) {
    static_cast<void>(operation);
  }

  GCP_LOG(DEBUG) << std::string(73, '=');

  // Terminate the process abruptly (after flushing the client log), without
  // dropping the database. This means we'll have a chance to examine it and
  // its server-side logs until they are garbage-collected.
  GCP_LOG(FATAL) << "Terminating after UpdateDatabaseDdl() failure";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
