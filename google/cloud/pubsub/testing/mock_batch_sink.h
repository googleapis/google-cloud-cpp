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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_SINK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_SINK_H

#include "google/cloud/pubsub/internal/batch_sink.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A class to mock pubsub_internal::BatchSink
 */
class MockBatchSink : public pubsub_internal::BatchSink {
 public:
  ~MockBatchSink() override = default;

  MOCK_METHOD(future<StatusOr<google::pubsub::v1::PublishResponse>>,
              AsyncPublish, (google::pubsub::v1::PublishRequest), (override));
  MOCK_METHOD(void, ResumePublish, (std::string const&), (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_SINK_H
