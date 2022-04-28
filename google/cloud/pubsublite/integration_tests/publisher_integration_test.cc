// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsublite/admin_connection.h"
#include "google/cloud/pubsublite/internal/location.h"
#include "google/cloud/pubsublite/options.h"
#include "google/cloud/pubsublite/publisher_connection.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/populate_grpc_options.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/version.h"
#include <google/cloud/pubsublite/v1/admin.pb.h>
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using google::cloud::pubsub::MessageBuilder;

using google::cloud::pubsub::PublisherConnection;
using google::cloud::pubsublite::v1::CreateTopicRequest;
using google::cloud::pubsublite::v1::DeleteTopicRequest;
using google::cloud::pubsublite::v1::ListTopicsRequest;
using google::cloud::pubsublite_internal::MakeLocation;

int constexpr kNumMessages = 10000;
int constexpr kThroughputCapacityMiB = 4;
std::int64_t constexpr kPartitionStorage =
    static_cast<std::int64_t>(1024) * 1024 * 1024 * 30;

class PublisherIntegrationTest : public testing_util::IntegrationTest {
 protected:
  PublisherIntegrationTest()  {
    auto locs = std::vector<std::string>{
      "us-central1-a", "us-central1-b", "us-central1-c", "us-east1-b",
      "us-east1-c",    "us-east4-b",    "us-east4-c",    "us-west1-a",
      "us-west1-c",    "us-west2-b",    "us-west2-c",    "us-west3-a",
      "us-west3-b",    "us-west4-a",    "us-west4-b",
  };
  project_id_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  srand(static_cast<unsigned int>(time(nullptr)));
  location_id_ = locs[rand() % locs.size()];

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto topic_id =
      google::cloud::pubsub_testing::RandomTopicId(generator);
  admin_connection_ =
      MakeAdminServiceConnection(google::cloud::internal::PopulateCommonOptions(
          google::cloud::internal::PopulateGrpcOptions(
              Options{}.set<EndpointOption>(
                  MakeLocation(location_id_)->GetCloudRegion().ToString() +
                  "-pubsublite.googleapis.com"),
              ""),
          /*endpoint_env_var=*/{}, /*emulator_env_var=*/{},
          "pubsublite.googleapis.com"));

  GarbageCollect();

  CreateTopicRequest req;
  req.set_parent("projects/" + project_id_ + "/locations/" + location_id_);
  req.set_topic_id(topic_id);
  req.mutable_topic()->mutable_partition_config()->set_count(3);
  req.mutable_topic()
      ->mutable_partition_config()
      ->mutable_capacity()
      ->set_publish_mib_per_sec(kThroughputCapacityMiB);
  req.mutable_topic()
      ->mutable_partition_config()
      ->mutable_capacity()
      ->set_subscribe_mib_per_sec(kThroughputCapacityMiB);
  req.mutable_topic()->mutable_retention_config()->set_per_partition_bytes(
      kPartitionStorage);
  EXPECT_TRUE(admin_connection_->CreateTopic(std::move(req)));
  auto topic = Topic{project_id_, location_id_, topic_id};
  topic_name_ = topic.FullName();
  publisher_ = *MakePublisherConnection(topic,
                                  Options{});
  }

  ~PublisherIntegrationTest() override {
    DeleteTopicRequest req;
    req.set_name(topic_name_);
    admin_connection_->DeleteTopic(std::move(req));
  }

  void GarbageCollect() {
    ListTopicsRequest req;
    req.set_parent("projects/" + project_id_ + "/locations/" + location_id_);
    auto topics = admin_connection_->ListTopics(std::move(req));
    for (auto const& topic : topics) {
      DeleteTopicRequest delete_req;
      delete_req.set_name(topic->name());
      admin_connection_->DeleteTopic(std::move(delete_req));
    }
  }

  std::shared_ptr<AdminServiceConnection> admin_connection_;
  std::string project_id_;
  std::string location_id_;
  std::string topic_name_;
  std::unique_ptr<PublisherConnection> publisher_;
};

TEST_F(PublisherIntegrationTest, BasicGoodWithoutKey) {
  std::vector<future<StatusOr<std::string>>> results;
  for (unsigned int i = 0; i != kNumMessages; ++i) {
    results.push_back(publisher_->Publish(
        {MessageBuilder{}.SetData("abcded-" + std::to_string(i)).Build()}));
  }
  for (unsigned i = 0; i != kNumMessages; ++i) {
    EXPECT_TRUE(results[i].get());
  }
}

TEST_F(PublisherIntegrationTest, BasicGoodWithKey) {
  std::vector<future<StatusOr<std::string>>> results;
  for (unsigned int i = 0; i != kNumMessages; ++i) {
    results.push_back(
        publisher_->Publish({MessageBuilder{}
                                 .SetData("abcded-" + std::to_string(i))
                                 .SetOrderingKey("key")
                                 .Build()}));
  }
  for (unsigned i = 0; i != kNumMessages; ++i) {
    EXPECT_TRUE(results[i].get());
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
