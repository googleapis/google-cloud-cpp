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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

PublisherConnection::~PublisherConnection() = default;

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultPublisherStub(options, /*channel_id=*/0);
  return pubsub_internal::MakePublisherConnection(std::move(topic),
                                                  std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class PublisherConnectionImpl : public pubsub::PublisherConnection {
 public:
  explicit PublisherConnectionImpl(
      pubsub::Topic topic, std::shared_ptr<pubsub_internal::PublisherStub> stub)
      : topic_(std::move(topic)),
        topic_full_name_(topic_.FullName()),
        stub_(std::move(stub)) {}

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    grpc::ClientContext context;
    google::pubsub::v1::PublishRequest request;
    request.set_topic(topic_full_name_);
    *request.add_messages() = pubsub_internal::ToProto(std::move(p.message));
    auto r = stub_->Publish(context, request);
    using Result = StatusOr<std::string>;
    if (!r) return make_ready_future<Result>(std::move(r).status());
    if (r->message_ids_size() != 1) {
      return make_ready_future<Result>(
          Status(StatusCode::kUnknown, "mismatched message id count"));
    }
    return make_ready_future<Result>(std::move(*r->mutable_message_ids(0)));
  }

 private:
  pubsub::Topic topic_;
  std::string topic_full_name_;
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
};
}  // namespace

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, std::shared_ptr<PublisherStub> stub) {
  return std::make_shared<PublisherConnectionImpl>(std::move(topic),
                                                   std::move(stub));
}
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
