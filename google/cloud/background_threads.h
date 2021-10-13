// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * A object representing the background threads available to a Client.
 */
class BackgroundThreads {
 public:
  virtual ~BackgroundThreads() = default;

  /// The completion queue used for the background operations.
  virtual CompletionQueue cq() const = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_H
