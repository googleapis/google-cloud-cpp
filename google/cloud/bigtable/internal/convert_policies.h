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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONVERT_POLICIES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONVERT_POLICIES_H

#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns `Options` with `GrpcSetupOption` and `GrpcSetupPollOption` set.
 *
 * The initial state of the policies are used to define these functions. It is
 * possible, although unlikely, that we are given a policy with a `Setup()`
 * function that changes depending on the loop iteration. Because we are
 * separating the policy from the configuration, we cannot perfectly handle this
 * case. The safest bet is to always apply the initial `Setup()`.
 *
 * While we would like to do this:
 *     Loop 1 { Setup 1, Setup 2, ... }
 *     Loop 2 { Setup 1, Setup 2, ... }
 *     ...
 *
 * It is much better to do this:
 *     Loop 1 { Setup 1, Setup 1, ... }
 *     Loop 2 { Setup 1, Setup 1, ... }
 *     ...
 *
 * Than this:
 *     Loop 1 { Setup   1, Setup   2, ..., Setup N }
 *     Loop 2 { Setup N+1, Setup N+2, ..., Setup M }
 *     ...
 */
Options MakeGrpcSetupOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> backoff,
    std::shared_ptr<bigtable::PollingPolicy> polling);

Options MakeInstanceAdminOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> backoff,
    std::shared_ptr<bigtable::PollingPolicy> polling);

Options MakeTableAdminOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> backoff,
    std::shared_ptr<bigtable::PollingPolicy> polling);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CONVERT_POLICIES_H
