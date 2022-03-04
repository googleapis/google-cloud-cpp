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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LIFECYCLE_INTERFACE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LIFECYCLE_INTERFACE_H

#include "google/cloud/pubsublite/internal/base_interface.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class LifecycleInterface : public BaseInterface {
 public:
  /**
   * Start the streaming RPC.
   *
   * The future returned by this function is satisfied when the stream
   * is successfully shut down (in which case in contains an ok status),
   * or when the retry policies to resume the stream are exhausted. The
   * latter includes the case where the stream fails with a permanent
   * error.
   *
   * While the stream is usable immediately after this function returns,
   * any outstanding futures will fail until the stream is initialized
   * successfully.
   */
  virtual future<Status> Start() = 0;

  /**
   * Finishes the streaming RPC.
   *
   * This will cause any outstanding outstanding futures to fail. This may be
   * called while an operation of an object of this class is outstanding.
   * Internally, the class will manage waiting on futures on a
   * gRPC stream before calling `Finish` on its underlying stream as per
   * `google::cloud::AsyncStreamingReadWriteRpc`. If the class is currently in a
   * retry loop, this will terminate the retry loop and then satisfy the
   * returned future. If the class has a present internal outstanding future,
   * this call will satisfy the returned future only after the internal
   * operation(s) finish.
   */
  virtual future<void> Shutdown() = 0;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LIFECYCLE_INTERFACE_H
