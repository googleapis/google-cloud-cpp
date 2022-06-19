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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_WAIT_FOR_CONSISTENCY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_WAIT_FOR_CONSISTENCY_H

#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Polls until a table is consistent, or until the polling policy has expired.
 *
 * @param cq the completion queue that will execute the asynchronous
 *    calls. The application must ensure that one or more threads are
 *    blocked on `cq.Run()`.
 * @param client the Table Admin client.
 * @param table_name the fully qualified name of the table. Values are of the
 *     form: `projects/{project}/instances/{instance}/tables/{table}`.
 * @param consistency_token the consistency token of the table.
 * @param options (optional) configuration options. Users who wish to modify the
 *     default polling behavior can supply a custom polling policy with
 *     `BigtableTableAdminPollingPolicyOption`. Note that the client's polling
 *     policy is not used for this operation.
 * @return the consistency status for the table. The status is OK if and only if
 *     the table is consistent.
 */
future<Status> AsyncWaitForConsistency(CompletionQueue cq,
                                       BigtableTableAdminClient client,
                                       std::string table_name,
                                       std::string consistency_token,
                                       Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_WAIT_FOR_CONSISTENCY_H
