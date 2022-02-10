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

#include "google/cloud/bigtable/internal/admin_client_params.h"
#include "google/cloud/grpc_options.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

AdminClientParams::AdminClientParams(Options opts) : options(std::move(opts)) {
  // If the CQ option is set, the user is running the background threads.
  // We can use their CQ.
  if (options.has<GrpcCompletionQueueOption>()) {
    cq = options.get<GrpcCompletionQueueOption>();
  } else {
    background_threads =
        google::cloud::internal::MakeBackgroundThreadsFactory(options)();
    cq = background_threads->cq();

    // Set this option so that the Connection will know not to create any
    // new threads. We will just run the CQ on our own threads.
    options.set<GrpcCompletionQueueOption>(cq);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
