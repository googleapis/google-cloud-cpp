// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/project.h"
#include "absl/strings/match.h"
#include <fstream>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
namespace examples {

google::cloud::testing_util::Commands::value_type CreatePublisherCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    PublisherCommand const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <topic-id>";
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    Topic const topic(argv.at(0), argv.at(1));
    argv.erase(argv.begin(), argv.begin() + 2);
    google::cloud::pubsub::Publisher client(
        google::cloud::pubsub::MakePublisherConnection(topic));
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

google::cloud::testing_util::Commands::value_type CreateSubscriberCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SubscriberCommand const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <subscription-id>";
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    google::cloud::pubsub::Subscriber client(
        google::cloud::pubsub::MakeSubscriberConnection(
            pubsub::Subscription(argv.at(0), argv.at(1))));
    argv.erase(argv.begin(), argv.begin() + 2);
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

google::cloud::testing_util::Commands::value_type CreateSchemaServiceCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SchemaServiceCommand const& command) {
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
    google::cloud::pubsub::SchemaServiceClient client(
        google::cloud::pubsub::MakeSchemaServiceConnection());
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomTopicId(generator,
                                                      "cloud-cpp-samples");
}

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSubscriptionId(
      generator, "cloud-cpp-samples");
}

std::string RandomSnapshotId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSnapshotId(generator,
                                                         "cloud-cpp-samples");
}

std::string RandomSchemaId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSchemaId(generator,
                                                       "cloud-cpp-samples");
}

std::string ReadFile(std::string const& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) throw std::runtime_error("Cannot open file: " + path);
  ifs.exceptions(std::ios::badbit);
  return std::string{std::istreambuf_iterator<char>{ifs.rdbuf()}, {}};
}

std::pair<std::string, std::string> CommitSchemaWithRevisionsForTesting(
    google::cloud::pubsub::SchemaServiceClient& client,
    std::string const& project_id, std::string const& schema_id,
    std::string const& schema_file, std::string const& revised_schema_file,
    std::string const& type) {
  std::string const initial_definition = ReadFile(schema_file);
  std::string const revised_definition = ReadFile(revised_schema_file);
  auto const schema_type = type == "AVRO"
                               ? google::pubsub::v1::Schema::AVRO
                               : google::pubsub::v1::Schema::PROTOCOL_BUFFER;

  google::pubsub::v1::CreateSchemaRequest create_request;
  create_request.set_parent(google::cloud::Project(project_id).FullName());
  create_request.set_schema_id(schema_id);
  create_request.mutable_schema()->set_type(schema_type);
  create_request.mutable_schema()->set_definition(initial_definition);
  auto schema = client.CreateSchema(create_request);
  if (!schema) throw std::move(schema).status();
  auto first_revision_id = schema->revision_id();

  google::pubsub::v1::CommitSchemaRequest commit_request;
  std::string const name =
      google::cloud::pubsub::Schema(project_id, schema_id).FullName();
  commit_request.set_name(name);
  commit_request.mutable_schema()->set_name(name);
  commit_request.mutable_schema()->set_type(schema_type);
  commit_request.mutable_schema()->set_definition(revised_definition);
  schema = client.CommitSchema(commit_request);
  if (!schema) throw std::move(schema).status();
  auto last_revision_id = schema->revision_id();

  return {std::move(first_revision_id), std::move(last_revision_id)};
}

void CleanupSchemas(google::cloud::pubsub::SchemaServiceClient& schema_admin,
                    std::string const& project_id, absl::Time const& time_now) {
  auto const parent = google::cloud::Project(project_id).FullName();
  for (auto& schema : schema_admin.ListSchemas(parent)) {
    if (!schema) continue;
    if (!absl::StartsWith(schema->name(), "cloud-cpp-samples")) continue;

    auto const schema_create_time =
        google::cloud::internal::ToAbslTime(schema->revision_create_time());
    if (schema_create_time < time_now - absl::Hours(48)) {
      google::pubsub::v1::DeleteSchemaRequest request;
      request.set_name(schema->name());
      (void)schema_admin.DeleteSchema(request);
    }
  }
}

}  // namespace examples
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
