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

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

TEST(MessageIntegrationTest, FireAndForget) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  Topic topic(project_id, pubsub_testing::RandomTopicId(generator));
  Subscription subscription(project_id,
                            pubsub_testing::RandomSubscriptionId(generator));

  auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
  auto subscription_admin =
      SubscriptionAdminClient(MakeSubscriptionAdminConnection());

  auto topic_metadata = topic_admin.CreateTopic(TopicMutationBuilder(topic));
  ASSERT_THAT(topic_metadata, AnyOf(StatusIs(StatusCode::kOk),
                                    StatusIs(StatusCode::kAlreadyExists)));

  struct Cleanup {
    std::function<void()> action;
    explicit Cleanup(std::function<void()> a) : action(std::move(a)) {}
    ~Cleanup() { action(); }
  };
  Cleanup cleanup_topic(
      [topic_admin, &topic]() mutable { topic_admin.DeleteTopic(topic); });

  auto subscription_metadata = subscription_admin.CreateSubscription(
      topic, subscription,
      SubscriptionMutationBuilder{}.set_ack_deadline(std::chrono::minutes(2)));
  ASSERT_THAT(
      subscription_metadata,
      AnyOf(StatusIs(StatusCode::kOk), StatusIs(StatusCode::kAlreadyExists)));

  std::mutex mu;
  std::condition_variable cv;
  std::set<std::string> received;
  Status subscription_result;
  std::set<std::string> published;
  std::vector<Status> publish_errors;
  auto constexpr kMinimumMessages = 10;

  auto publisher = Publisher(MakePublisherConnection(topic, {}));
  auto subscriber = Subscriber(MakeSubscriberConnection());
  internal::AutomaticallyCreatedBackgroundThreads background(4);
  {
    (void)subscriber
        .Subscribe(subscription,
                   [&](Message const& m, AckHandler h) {
                     std::move(h).ack();
                     std::unique_lock<std::mutex> lk(mu);
                     std::cout << "received " << m.message_id() << std::endl;
                     received.insert(m.message_id());
                     lk.unlock();
                     cv.notify_one();
                   })
        .then([&](future<Status> f) {
          std::unique_lock<std::mutex> lk(mu);
          subscription_result = f.get();
          cv.notify_one();
        });

    std::vector<future<void>> pending;
    for (int i = 0; i != kMinimumMessages; ++i) {
      pending.push_back(
          publisher
              .Publish(MessageBuilder{}
                           .SetAttributes({{"index", std::to_string(i)}})
                           .Build())
              .then([&](future<StatusOr<std::string>> f) {
                std::unique_lock<std::mutex> lk(mu);
                auto s = f.get();
                if (!s) {
                  publish_errors.push_back(std::move(s).status());
                  return;
                }
                published.insert(*std::move(s));
              }));
    }
    publisher.Flush();
    for (auto& p : pending) p.get();
  }
  {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return received.size() >= published.size(); });
  }
  EXPECT_THAT(publish_errors, ElementsAre());
  EXPECT_THAT(received, ElementsAreArray(published));

  auto delete_response = subscription_admin.DeleteSubscription(subscription);
  EXPECT_THAT(delete_response, AnyOf(StatusIs(StatusCode::kOk),
                                     StatusIs(StatusCode::kNotFound)));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
