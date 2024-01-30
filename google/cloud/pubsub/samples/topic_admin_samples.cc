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
using ::google::cloud::pubsub::examples::UsingEmulator;
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

void GetTopic(google::cloud::pubsub_admin::TopicAdminClient client,
              std::vector<std::string> const& argv) {
  //! [get-topic]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.GetTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic information was successfully retrieved: "
              << topic->DebugString() << "\n";
  }
  //! [get-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateTopic(google::cloud::pubsub_admin::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [update-topic]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    google::pubsub::v1::UpdateTopicRequest request;
    request.mutable_topic()->set_name(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    request.mutable_topic()->mutable_labels()->insert(
        {"test-key", "test-value"});
    *request.mutable_update_mask()->add_paths() = "labels";
    auto topic = client.UpdateTopic(request);
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully updated: " << topic->DebugString()
              << "\n";
  }
  //! [update-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopics(google::cloud::pubsub_admin::TopicAdminClient client,
                std::vector<std::string> const& argv) {
  //! [list-topics]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string const& project_id) {
    int count = 0;
    for (auto& topic : client.ListTopics("projects/" + project_id)) {
      if (!topic) throw std::move(topic).status();
      std::cout << "Topic Name: " << topic->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No topics found in project " << project_id << "\n";
    }
  }
  //! [list-topics]
  (std::move(client), argv.at(0));
}

void ListTopicSubscriptions(
    google::cloud::pubsub_admin::TopicAdminClient client,
    std::vector<std::string> const& argv) {
  //! [list-topic-subscriptions]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::TopicAdminClient client, std::string const& project_id,
     std::string const& topic_id) {
    auto const topic = pubsub::Topic(project_id, topic_id);
    std::cout << "Subscription list for topic " << topic << ":\n";
    for (auto& name : client.ListTopicSubscriptions(topic.FullName())) {
      if (!name) throw std::move(name).status();
      std::cout << "  " << *name << "\n";
    }
  }
  //! [list-topic-subscriptions]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopicSnapshots(google::cloud::pubsub_admin::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [list-topic-snapshots]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    std::cout << "Snapshot list for topic " << topic << ":\n";
    for (auto& name : client.ListTopicSnapshots(topic.FullName())) {
      if (!name) throw std::move(name).status();
      std::cout << "  " << *name << "\n";
    }
  }
  //! [list-topic-snapshots]
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

  using ::google::cloud::StatusCode;
  auto ignore_emulator_failures =
      [](auto lambda, StatusCode code = StatusCode::kUnimplemented) {
        try {
          lambda();
        } catch (google::cloud::Status const& s) {
          if (UsingEmulator() && s.code() == code) return;
          throw;
        } catch (...) {
          throw;
        }
      };

  google::cloud::pubsub_admin::TopicAdminClient topic_admin_client(
      google::cloud::pubsub_admin::MakeTopicAdminConnection());

  std::cout << "\nRunning CreateTopic() sample [1]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  // Since the topic was created already, this should return kAlreadyExists.
  std::cout << "\nRunning CreateTopic() sample [2]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning GetTopic() sample" << std::endl;
  GetTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning UpdateTopic() sample" << std::endl;
  ignore_emulator_failures(
      [&] {
        UpdateTopic(topic_admin_client, {project_id, topic_id});
      },
      StatusCode::kInvalidArgument);

  std::cout << "\nRunning ListTopics() sample" << std::endl;
  ListTopics(topic_admin_client, {project_id});

  std::cout << "\nRunning ListTopicSnapshots() sample" << std::endl;
  ListTopicSnapshots(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning ListTopicSubscriptions() sample" << std::endl;
  ListTopicSubscriptions(topic_admin_client, {project_id, topic_id});

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
      CreateTopicAdminCommand("get-topic", {"project-id", "topic-id"},
                              GetTopic),
      CreateTopicAdminCommand("update-topic", {"project-id", "topic-id"},
                              UpdateTopic),
      CreateTopicAdminCommand("list-topics", {"project-id"}, ListTopics),
      CreateTopicAdminCommand("list-topic-subscriptions",
                              {"project-id", "topic-id"},
                              ListTopicSubscriptions),
      CreateTopicAdminCommand("list-topic-snapshots",
                              {"project-id", "topic-id"}, ListTopicSnapshots),
      CreateTopicAdminCommand("delete-topic", {"project-id", "topic-id"},
                              DeleteTopic),
      {"auto", AutoRun},
  });

  return example.Run(argc, argv);
}
