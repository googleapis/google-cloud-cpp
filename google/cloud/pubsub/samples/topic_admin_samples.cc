// Copyright 2024 Google LLC
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

#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <sstream>

namespace {

using ::google::cloud::pubsub::examples::RandomTopicId;

using TopicAdminCommand =
    std::function<void(::google::cloud::pubsub_admin::TopicAdminClient,
                       std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateTopicAdminCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    TopicAdminCommand const& command) {
  auto adapter = [=](std::vector<std::string> const& argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    google::cloud::pubsub_admin::TopicAdminClient client(
        google::cloud::pubsub_admin::MakeTopicAdminConnection());
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

void CreateTopic(google::cloud::pubsub_admin::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [create-topic]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.CreateTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [create-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteTopic(google::cloud::pubsub_admin::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [delete-topic]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string const& project_id,
     std::string const& topic_id) {
    auto status =
        client.DeleteTopic(pubsub::Topic(project_id, topic_id).FullName());
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The topic was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The topic was successfully deleted\n";
  }
  //! [delete-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const topic = google::cloud::pubsub::Topic(project_id, topic_id);

  google::cloud::pubsub_admin::TopicAdminClient topic_admin_client(
      google::cloud::pubsub_admin::MakeTopicAdminConnection());

  std::cout << "\nRunning CreateTopic() sample" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning DeleteTopic() sample" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateTopicAdminCommand("create-topic", {"project-id", "topic-id"},
                              CreateTopic),
      CreateTopicAdminCommand("delete-topic", {"project-id", "topic-id"},
                              DeleteTopic),
      {"auto", AutoRun},
  });

  return example.Run(argc, argv);
}
