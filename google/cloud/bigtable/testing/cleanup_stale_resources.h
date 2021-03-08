// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_CLEANUP_STALE_RESOURCES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_CLEANUP_STALE_RESOURCES_H

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/table_admin.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/**
 * Remove stale test tables.
 *
 * Tables matching the pattern from `CreateRandomTableId()` are created by
 * tests. This function removes such test tables if they are older than 2 days.
 * These typically are the result of a leak from one of the tests, repairing
 * such leaks is important, but (a) leaks are unavoidable if the test crashes or
 * times out, and (b) avoiding flakes caused by quota exhaustion is necessary
 * for healthy builds.
 */
Status CleanupStaleTables(google::cloud::bigtable::TableAdmin admin);

/**
 * Remove stale test backups.
 *
 * Backups matching the pattern from `CreateRandomBackupId()` are created by
 * tests. This function removes such test backups if they are older than 2 days.
 * These typically are the result of a leak from one of the tests, repairing
 * such leaks is important, but (a) leaks are unavoidable if the test crashes or
 * times out, and (b) avoiding flakes caused by quota exhaustion is necessary
 * for healthy builds.
 */
Status CleanupStaleBackups(google::cloud::bigtable::TableAdmin admin);

/**
 * Remove stale test instances.
 *
 * Backups matching the pattern from `CreateRandomInstanceId()` are created by
 * tests. This function removes such test instances if they are older than 2
 * days. These typically are the result of a leak from one of the tests,
 * repairing such leaks is important, but (a) leaks are unavoidable if the test
 * crashes or times out, and (b) avoiding flakes caused by quota exhaustion is
 * necessary for healthy builds.
 */
Status CleanupStaleInstances(google::cloud::bigtable::InstanceAdmin admin);

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_CLEANUP_STALE_RESOURCES_H
