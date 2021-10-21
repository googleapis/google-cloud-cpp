// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H

#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

using PollingPolicy = ::google::cloud::PollingPolicy;

template <typename Retry = LimitedTimeRetryPolicy,
          typename Backoff = ExponentialBackoffPolicy>
using GenericPollingPolicy =
    ::google::cloud::GenericPollingPolicy<Retry, Backoff>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H
