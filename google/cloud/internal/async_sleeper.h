// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SLEEPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SLEEPER_H

#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Define the interface for AsyncSleeper.
 *
 * This interface provides the framework expected by the client library to
 * invoke asynchronous sleeper methods.
 *
 */
class AsyncSleeper {
 public:
  virtual ~AsyncSleeper() = default;

  /**
   * @return an empty google::cloud::future to indicate the end of the passed
   * duration.
   */
  virtual future<void> sleep(std::chrono::duration) = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SLEEPER_H
