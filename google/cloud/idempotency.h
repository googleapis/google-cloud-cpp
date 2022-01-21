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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IDEMPOTENCY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IDEMPOTENCY_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Whether a request is [idempotent][wikipedia-idempotence].
 *
 * When a RPC fails with a retryable error, the `google-cloud-cpp` client
 * libraries automatically retry the RPC **if** the RPC is
 * [idempotent][wikipedia-idempotence].  For each service, the library define
 * a policy that determines whether a given request is idempotent.  In many
 * cases this can be determined statically, for example, read-only operations
 * are always idempotent. In some cases, the contents of the request may need to
 * be examined to determine if the operation is idempotent. For example,
 * performing operations with pre-conditions, such that the pre-conditions
 * change when the operation succeed, is typically idempotent.
 *
 * Applications may override the default idempotency policy, though we
 * anticipate that this would be needed only in very rare circumstances. A few
 * examples include:
 *
 * - In some services deleting "the most recent" entry may be idempotent if the
 *   system has been configured to keep no history or versions, as the deletion
 *   may succeed only once. In contrast, deleting "the most recent entry" is
 *   **not** idempotent if the system keeps multiple versions. Google Cloud
 *   Storage or Bigtable can be configured either way.
 * - In some applications, creating a duplicate entry may be acceptable as the
 *   system will deduplicate them later.  In such systems it may be preferable
 *   to retry the operation even though it is not idempotent.
 *
 * [wikipedia-idempotence]: https://en.wikipedia.org/wiki/Idempotence
 */
enum class Idempotency {
  /// The operation is idempotent and can be retried after a transient failure.
  kIdempotent,
  /// The operation is not idempotent and should **not** be retried after a
  /// transient failure.
  kNonIdempotent
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IDEMPOTENCY_H
