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

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsublite/admin_client.h"
#include "google/cloud/pubsublite/endpoint.h"
#include "google/cloud/pubsublite/options.h"
#include "google/cloud/pubsublite/publisher_connection.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/version.h"
#include <google/cloud/pubsublite/v1/admin.pb.h>
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::pubsub::MessageBuilder;
using ::google::cloud::pubsub::Publisher;
using ::google::cloud::pubsub::PublisherConnection;
using ::std::chrono::hours;
using ::std::chrono::system_clock;

auto constexpr kNumMessages = 125;
auto constexpr kThroughputCapacityMiB = 4;
auto constexpr kGiB = static_cast<std::int64_t>(1024 * 1024 * 1024LL);
auto constexpr kPartitionStorage = 30 * kGiB;
auto constexpr kMaxNumMessagesPerBatch = 25;
auto constexpr kTopicRegex =
    R"re(^projects\/\d*\/locations\/[a-z0-9\-]*\/topics\/pub-int-test[-_]\d{4}[-_]\d{2}[-_]\d{2}[-_])re";

std::string TestTopicPrefix(system_clock::time_point tp) {
  return "pub-int-test-" + google::cloud::internal::FormatUtcDate(tp) + "-";
}

std::string RandomTopicId() {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  std::size_t const max_topic_size = 70;
  auto const topic_prefix = TestTopicPrefix(system_clock::now());
  auto size = static_cast<int>(max_topic_size - 1 - topic_prefix.size());
  return topic_prefix +
         google::cloud::internal::Sample(
             generator, size, "abcdefghijlkmnopqrstuvwxyz0123456789_-") +
         google::cloud::internal::Sample(
             generator, 1, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

void GarbageCollect(AdminServiceClient client, std::string const& parent) {
  auto stale_prefix =
      parent + "/topics/" + TestTopicPrefix(system_clock::now() - hours(48));
  auto topics = client.ListTopics(parent);
  for (auto const& topic : topics) {
    ASSERT_STATUS_OK(topic);
    if (!std::regex_search(topic->name(), std::regex{kTopicRegex})) continue;
    if (topic->name() < stale_prefix) {
      client.DeleteTopic(topic->name());
    }
  }
}

class PublisherIntegrationTest : public testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());
    auto const location_id = GetEnv("GOOGLE_CLOUD_CPP_TEST_ZONE").value_or("");
    ASSERT_FALSE(location_id.empty());
    auto const parent =
        std::string{"projects/"} + project_id + "/locations/" + location_id;
    auto ep = EndpointFromZone(location_id);
    ASSERT_STATUS_OK(ep);

    auto options = Options{}.set<EndpointOption>(*ep).set<AuthorityOption>(*ep);
    admin_connection_ = MakeAdminServiceConnection(std::move(options));
    auto client = AdminServiceClient(admin_connection_);

    GarbageCollect(client, parent);

    auto const topic_id = RandomTopicId();

    google::cloud::pubsublite::v1::Topic t;
    t.mutable_partition_config()->set_count(3);
    t.mutable_retention_config()->set_per_partition_bytes(kPartitionStorage);
    auto& capacity = *t.mutable_partition_config()->mutable_capacity();
    capacity.set_publish_mib_per_sec(kThroughputCapacityMiB);
    capacity.set_subscribe_mib_per_sec(kThroughputCapacityMiB);
    ASSERT_STATUS_OK(client.CreateTopic(parent, t, topic_id));

    auto topic = Topic{project_id, location_id, topic_id};
    auto publisher_connection = MakePublisherConnection(
        std::move(topic),
        Options{}.set<MaxBatchMessagesOption>(kMaxNumMessagesPerBatch));
    ASSERT_STATUS_OK(publisher_connection);
    publisher_connection_ = *std::move(publisher_connection);
  }

  void TearDown() override {
    auto client = AdminServiceClient(admin_connection_);
    (void)client.DeleteTopic(topic_name_);
  }

  std::string topic_name_;
  std::shared_ptr<AdminServiceConnection> admin_connection_;
  std::shared_ptr<PublisherConnection> publisher_connection_;
};

TEST_F(PublisherIntegrationTest, BasicGoodWithoutKey) {
  auto publisher = Publisher(publisher_connection_);
  std::vector<future<StatusOr<std::string>>> results;
  for (int i = 0; i != kNumMessages; ++i) {
    results.push_back(publisher.Publish(
        {MessageBuilder{}.SetData("abcded-" + std::to_string(i)).Build()}));
  }
  for (int i = 0; i != kNumMessages; ++i) {
    EXPECT_STATUS_OK(results[i].get());
  }
}

TEST_F(PublisherIntegrationTest, BasicGoodWithKey) {
  auto publisher = Publisher(publisher_connection_);
  std::vector<future<StatusOr<std::string>>> results;
  for (int i = 0; i != kNumMessages; ++i) {
    results.push_back(
        publisher.Publish({MessageBuilder{}
                               .SetData("abcded-" + std::to_string(i))
                               .SetOrderingKey("key")
                               .Build()}));
  }
  for (int i = 0; i != kNumMessages; ++i) {
    EXPECT_STATUS_OK(results[i].get());
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
