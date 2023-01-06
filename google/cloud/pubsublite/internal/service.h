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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_H

#include "google/cloud/future.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `Service` is anything that can start, run for a while, possibly have other
 * operations invoked on it, possibly with an error, and then can shutdown.
 *
 * Examples include resumable streaming RPCs of different types, and
 * compositions of these streaming RPCs.
 */
class Service {
 protected:
  Service() = default;

 public:
  virtual ~Service() = default;
  Service(Service const&) = delete;
  Service& operator=(Service const&) = delete;
  Service(Service&&) = delete;
  Service& operator=(Service&&) = delete;

  /**
   * Starts the lifecycle of the object.
   *
   * The future returned by this function is satisfied when the object
   * encounters an error during one of its operations or when `Shutdown` is
   * called on it. The value the future is satisfied with details what happened
   * to the object, i.e., `Shutdown` was called, encountered a permanent error,
   * etc.
   *
   * Must be called before any other method and may only be called once.
   *
   * @note The above restriction applies to all additional member functions
   * present in derived classes, if any.
   */
  virtual future<Status> Start() = 0;

  /**
   * This causes the object to reach its shutdown state if not already in it.
   *
   * This will cause any outstanding futures to fail. This may be
   * called while an asynchronous operation of an object of this class is
   * outstanding. Internally, the class will manage waiting on futures of any
   * dependencies. If the class has a present internal outstanding future, this
   * call will satisfy the returned future only after the internal operation(s)
   * finish.
   *
   * Must be called before destroying this object if `Start` was called and may
   * only be called once.
   */
  virtual future<void> Shutdown() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_H
