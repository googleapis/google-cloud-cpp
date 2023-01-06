// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_DATABASES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_DATABASES_H

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/version.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Drop all databases whose names indicate that they were created on or
// before the (UTC) day that contains @p `tp`. This is useful to clean
// up databases created by previous tests that crashed before having a
// chance to cleanup after themselves.
Status CleanupStaleDatabases(
    google::cloud::spanner_admin::DatabaseAdminClient admin_client,
    std::string const& project_id, std::string const& instance_id,
    std::chrono::system_clock::time_point tp);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_DATABASES_H
