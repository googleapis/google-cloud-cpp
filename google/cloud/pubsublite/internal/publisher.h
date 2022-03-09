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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_H

#include "google/cloud/pubsublite/internal/service.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include <google/cloud/pubsublite/v1/common.pb.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

template <class ReturnT>
class Publisher : public Service {
 public:
  virtual future<StatusOr<ReturnT>> Publish(
      google::cloud::pubsublite::v1::PubSubMessage m) = 0;

  /**
   * Forcibly publishes any batched messages.
   *
   * As applications can configure a `Publisher` to buffer messages, it is
   * sometimes useful to flush them before any of the normal criteria to send
   * the RPCs is met.
   *
   * @note This function does not return any status or error codes, the
   *     application can use the `future<StatusOr<std::string>>` returned in
   *     each `Publish()` call to find out what the results are.
   */
  virtual void Flush() = 0;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PUBLISHER_H
