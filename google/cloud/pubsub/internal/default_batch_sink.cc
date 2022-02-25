// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DefaultBatchSink::DefaultBatchSink(std::shared_ptr<PublisherStub> stub,
                                   CompletionQueue cq, Options opts)
    : stub_(std::move(stub)), cq_(std::move(cq)), options_(std::move(opts)) {}

future<StatusOr<google::pubsub::v1::PublishResponse>>
DefaultBatchSink::AsyncPublish(google::pubsub::v1::PublishRequest request) {
  internal::OptionsSpan span(options_);

  auto& stub = stub_;
  return internal::AsyncRetryLoop(
      options_.get<pubsub::RetryPolicyOption>()->clone(),
      options_.get<pubsub::BackoffPolicyOption>()->clone(),
      Idempotency::kIdempotent, cq_,
      [stub](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PublishRequest const& request) {
        return stub->AsyncPublish(cq, std::move(context), request);
      },
      std::move(request), __func__);
}

void DefaultBatchSink::ResumePublish(std::string const&) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
