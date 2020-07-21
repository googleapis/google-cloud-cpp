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
                                                  std::move(stub), options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class PublisherConnectionImpl : public pubsub::PublisherConnection {
 public:
  explicit PublisherConnectionImpl(
      pubsub::Topic topic, std::shared_ptr<pubsub_internal::PublisherStub> stub,
      pubsub::ConnectionOptions const& options)
      : topic_(std::move(topic)),
        topic_full_name_(topic_.FullName()),
        stub_(std::move(stub)),
        background_(options.background_threads_factory()()),
        cq_(background_->cq()) {}

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    auto context = absl::make_unique<grpc::ClientContext>();
    google::pubsub::v1::PublishRequest request;
    request.set_topic(topic_full_name_);
    *request.add_messages() = pubsub_internal::ToProto(std::move(p.message));
    return stub_->AsyncPublish(cq_, std::move(context), request)
        .then([](future<StatusOr<google::pubsub::v1::PublishResponse>> f)
                  -> StatusOr<std::string> {
          auto response = f.get();
          if (!response) return std::move(response).status();
          if (response->message_ids_size() != 1) {
            return Status(StatusCode::kUnknown, "mismatched message id count");
          }
          return std::move(*response->mutable_message_ids(0));
        });
  }

 private:
  pubsub::Topic topic_;
  std::string topic_full_name_;
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  std::unique_ptr<BackgroundThreads> background_;
  google::cloud::CompletionQueue cq_;
};
}  // namespace

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, std::shared_ptr<PublisherStub> stub,
    pubsub::ConnectionOptions const& options) {
  return std::make_shared<PublisherConnectionImpl>(std::move(topic),
                                                   std::move(stub), options);
}
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
