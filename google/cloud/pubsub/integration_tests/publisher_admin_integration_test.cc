// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

std::string RandomTopic(google::cloud::internal::DefaultPRNG& generator,
                        std::string const& prefix = "cloud-cpp-testing-") {
  return prefix + google::cloud::internal::Sample(generator, 32,
                                                  "abcdefghijklmnopqrstuvwxyz");
}

TEST(PublisherAdminIntegrationTest, PublisherCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto topic_id = RandomTopic(generator);

  auto publisher =
      pubsub_internal::CreateDefaultPublisherStub(ConnectionOptions{}, 0);
  auto create_response = [&publisher, &topic_id, &project_id] {
    google::pubsub::v1::Topic request;
    request.set_name("projects/" + project_id + "/topics/" + topic_id);
    grpc::ClientContext context;
    return publisher->CreateTopic(context, request);
  }();
  ASSERT_STATUS_OK(create_response);

  auto delete_response = [&publisher, &topic_id, &project_id] {
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic("projects/" + project_id + "/topics/" + topic_id);
    grpc::ClientContext context;
    return publisher->DeleteTopic(context, request);
  }();
  ASSERT_STATUS_OK(delete_response);
}

TEST(PublisherAdminIntegrationTest, CreateFailure) {
  auto connection_options =
      ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("localhost:1");
  auto publisher =
      pubsub_internal::CreateDefaultPublisherStub(ConnectionOptions{}, 0);
  auto create_response = [&publisher] {
    google::pubsub::v1::Topic request;
    grpc::ClientContext context;
    return publisher->CreateTopic(context, request);
  }();
  ASSERT_FALSE(create_response);
}

TEST(PublisherAdminIntegrationTest, DeleteFailure) {
  auto connection_options =
      ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("localhost:1");
  auto publisher =
      pubsub_internal::CreateDefaultPublisherStub(ConnectionOptions{}, 0);
  auto delete_response = [&publisher] {
    google::pubsub::v1::DeleteTopicRequest request;
    grpc::ClientContext context;
    return publisher->DeleteTopic(context, request);
  }();
  ASSERT_FALSE(delete_response.ok());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
