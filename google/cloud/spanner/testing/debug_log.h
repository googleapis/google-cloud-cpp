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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DEBUG_LOG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DEBUG_LOG_H

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Log everything we can after an UpdateDatabaseDdl() failure so that
 * we might have a chance to debug the apparent replays behind #4758.
 */
void LogUpdateDatabaseDdl(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    google::cloud::spanner::Database const& database,
    google::cloud::Status const& status);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_DEBUG_LOG_H
