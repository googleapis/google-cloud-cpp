// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsublite/publisher_connection_impl.h"

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::pubsublite_internal::Publisher;

PublisherConnectionImpl::PublisherConnectionImpl(
    std::unique_ptr<google::cloud::pubsublite_internal::Publisher<
        google::cloud::pubsublite::MessageMetadata>>
        publisher,
    Options const& opts)
    : publisher_{std::move(publisher)},
      service_composite_{publisher_.get()},
      message_transformer_{opts.get<PublishMessageTransformer>()} {
  service_composite_.Start();
}

PublisherConnectionImpl::~PublisherConnectionImpl() {
  service_composite_.Shutdown().get();
}

future<StatusOr<std::string>> PublisherConnectionImpl::Publish(
    PublishParams p) {
  auto pubsub_message = message_transformer_(std::move(p.message));
  if (!pubsub_message) {
    return make_ready_future(
        StatusOr<std::string>{std::move(pubsub_message).status()});
  }
  return publisher_->Publish(*std::move(pubsub_message))
      .then([](future<StatusOr<MessageMetadata>> f) -> StatusOr<std::string> {
        auto message_metadata = f.get();
        if (!message_metadata) return std::move(message_metadata).status();
        return message_metadata->Serialize();
      });
}

void PublisherConnectionImpl::Flush(FlushParams) { publisher_->Flush(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
