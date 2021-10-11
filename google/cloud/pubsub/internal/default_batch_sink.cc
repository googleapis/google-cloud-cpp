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

#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_NS {

DefaultBatchSink::DefaultBatchSink(std::shared_ptr<PublisherStub> stub,
                                   CompletionQueue cq, Options const& opts)
    : stub_(std::move(stub)),
      cq_(std::move(cq)),
      retry_policy_(opts.get<pubsub::RetryPolicyOption>()->clone()),
      backoff_policy_(opts.get<pubsub::BackoffPolicyOption>()->clone()) {}

future<StatusOr<google::pubsub::v1::PublishResponse>>
DefaultBatchSink::AsyncPublish(google::pubsub::v1::PublishRequest request) {
  auto& stub = stub_;
  return internal::AsyncRetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(),
      internal::Idempotency::kIdempotent, cq_,
      [stub](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PublishRequest const& request) {
        return stub->AsyncPublish(cq, std::move(context), request);
      },
      std::move(request), __func__);
}

void DefaultBatchSink::ResumePublish(std::string const&) {}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
