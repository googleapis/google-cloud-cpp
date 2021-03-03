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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_SINK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_SINK_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Define the interface to push message batches to Cloud Pub/Sub.
 *
 * Implements the behavior to push a batch of messages to Cloud Pub/Sub,
 * potentially queueing these batches and sending one at a time (for ordering
 * keys).
 */
class BatchSink {
 public:
  virtual ~BatchSink() = default;

  /// Asynchronously publishes a batch of messages.
  virtual future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::pubsub::v1::PublishRequest request) = 0;

  /// Resume publishing
  virtual void ResumePublish(std::string const& ordering_key) = 0;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCH_SINK_H
