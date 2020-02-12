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

namespace {
class PublisherConnectionImpl : public PublisherConnection {
 public:
  explicit PublisherConnectionImpl(
      std::shared_ptr<pubsub_internal::PublisherStub> stub)
      : stub_(std::move(stub)) {}

  ~PublisherConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      CreateTopicParams p) override {
    grpc::ClientContext context;
    return stub_->CreateTopic(context, p.topic);
  }

  ListTopicsRange ListTopics(ListTopicsParams p) override {
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project(std::move(p.project_id));
    auto& stub = stub_;
    return ListTopicsRange(
        std::move(request),
        [stub](google::pubsub::v1::ListTopicsRequest const& request) {
          grpc::ClientContext context;
          return stub->ListTopics(context, request);
        },
        [](google::pubsub::v1::ListTopicsResponse response) {
          std::vector<google::pubsub::v1::Topic> items;
          items.reserve(response.topics_size());
          for (auto& item : *response.mutable_topics()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  Status DeleteTopic(DeleteTopicParams p) override {
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic(p.topic.FullName());
    grpc::ClientContext context;
    return stub_->DeleteTopic(context, request);
  }

 private:
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
};
}  // namespace

PublisherConnection::~PublisherConnection() = default;

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultPublisherStub(options, /*channel_id=*/0);
  return std::make_shared<PublisherConnectionImpl>(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
