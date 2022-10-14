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

#include "google/cloud/pubsub/blocking_publisher.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"

namespace {

using ::google::cloud::pubsub::examples::RandomTopicId;

void BlockingPublish(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"blocking-publish <project-id> <topic-id>"};
  }
  //! [blocking-publish]
  namespace pubsub = ::google::cloud::pubsub;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto publisher =
        pubsub::BlockingPublisher(pubsub::MakeBlockingPublisherConnection());
    auto id = publisher.Publish(
        topic, pubsub::MessageBuilder().SetData("Hello World!").Build());
    if (!id) throw std::move(id).status();
    std::cout << "Hello World successfully published on topic "
              << topic.FullName() << " with id " << *id << "\n";
  }
  //! [blocking-publish]
  (argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const topic = google::cloud::pubsub::Topic(project_id, topic_id);

  google::cloud::pubsub::TopicAdminClient topic_admin_client(
      google::cloud::pubsub::MakeTopicAdminConnection());

  std::cout << "\nCreateTopic()" << std::endl;
  auto topic_metadata = topic_admin_client.CreateTopic(
      google::cloud::pubsub::TopicBuilder(topic));

  std::cout << "\nRunning BlockingPublish()" << std::endl;
  BlockingPublish({project_id, topic_id});

  std::cout << "\nDeleteTopic()" << std::endl;
  (void)topic_admin_client.DeleteTopic(topic);

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      {"blocking-publish", BlockingPublish},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
