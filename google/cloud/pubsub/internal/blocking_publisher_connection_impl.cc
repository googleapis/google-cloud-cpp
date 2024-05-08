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

#include "google/cloud/pubsub/internal/blocking_publisher_connection_impl.h"
#include "google/cloud/pubsub/internal/publisher_stub_factory.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;
using ::google::cloud::internal::RetryLoop;

BlockingPublisherConnectionImpl::BlockingPublisherConnectionImpl(
    std::unique_ptr<google::cloud::BackgroundThreads> background,
    std::shared_ptr<PublisherStub> stub, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      options_(std::move(options)) {}

StatusOr<std::string> BlockingPublisherConnectionImpl::Publish(
    PublishParams p) {
  auto const& current = internal::CurrentOptions();
  google::pubsub::v1::PublishRequest request;
  request.set_topic(p.topic.FullName());
  *request.add_messages() = ToProto(std::move(p.message));
  auto response = RetryLoop(
      current.get<pubsub::RetryPolicyOption>()->clone(),
      current.get<pubsub::BackoffPolicyOption>()->clone(),
      Idempotency::kIdempotent,
      [this](grpc::ClientContext& context, Options const& options,
             google::pubsub::v1::PublishRequest const& request) {
        return stub_->Publish(context, options, request);
      },
      current, request, __func__);
  if (!response) return std::move(response).status();
  if (response->message_ids_size() != 1) {
    return internal::InternalError("invalid response, mismatched ID count",
                                   GCP_ERROR_INFO());
  }
  return std::move(*response->mutable_message_ids(0));
}

Options BlockingPublisherConnectionImpl::options() { return options_; }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
