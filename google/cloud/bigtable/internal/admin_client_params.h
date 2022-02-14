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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ADMIN_CLIENT_PARAMS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ADMIN_CLIENT_PARAMS_H

#include "google/cloud/background_threads.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A helper class for `DefaultAdminClient` to process `Options` before they are
 * used to initialize its `BigtableTableAdminConnection`. The point is to save
 * the creation of a background thread. The class is factored out for testing
 * purposes.
 *
 * `TableAdmin::WaitForConsistency()` requires a CQ to run the
 * polling loop. We would like to run this CQ on the same background threads
 * that the Connection uses, instead of spinning off an extra thread, just
 * for this purpose.
 *
 * If the user supplies their own threads, we can use those to run the
 * polling loop.
 *
 * Otherwise, we will create and store the background threads
 * in this class. Then we will tell the Connection to use our threads for
 * its background work.
 *
 * The CQ will be stored in `options` as a `GrpcCompletionQueueOption`.
 */
struct AdminClientParams {
  explicit AdminClientParams(Options opts);

  std::unique_ptr<BackgroundThreads> background_threads;
  Options options;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ADMIN_CLIENT_PARAMS_H
