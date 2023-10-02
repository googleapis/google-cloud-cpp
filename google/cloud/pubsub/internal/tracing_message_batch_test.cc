// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/tracing_message_batch.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/testing/mock_message_batch.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeSpan;
using ::testing::SizeIs;

class TracingMessageBatchPeer {
 public:
  explicit TracingMessageBatchPeer(std::unique_ptr<TracingMessageBatch> child)
      : child_(std::move(child)) {}
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
  GetSpans() const {
    return child_->message_root_spans_;
  }

  std::unique_ptr<TracingMessageBatch> child_;
};
namespace {

TEST(TracingMessageBatch, SaveMessage) {
  auto span = MakeSpan("test span");
  opentelemetry::trace::Scope scope(span);
  auto mock = std::make_unique<pubsub_testing::MockMessageBatch>();
  EXPECT_CALL(*mock, SaveMessage);
  auto message_batch = std::make_unique<TracingMessageBatch>(std::move(mock));
  auto message = pubsub::MessageBuilder().SetData("test").Build();

  message_batch->SaveMessage(message);

  span->End();
  auto peer = TracingMessageBatchPeer(std::move(message_batch));
  auto spans = peer.GetSpans();
  EXPECT_THAT(spans, SizeIs(1));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
