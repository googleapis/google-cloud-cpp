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
  // If the user has not supplied a CQ, we will create the background threads
  // here and configure the Connection to use them.
  if (!options.has<GrpcCompletionQueueOption>()) {
    background_threads = internal::MakeBackgroundThreadsFactory(options)();
    options.set<GrpcCompletionQueueOption>(background_threads->cq());
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
