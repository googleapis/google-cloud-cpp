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

#include "google/cloud/pubsub/admin/subscription_admin_client.h"
#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/strings/match.h"
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/time_util.h>
#include <sstream>

using google::cloud::pubsub::examples::Cleanup;
using google::cloud::pubsub::examples::CommitSchemaWithRevisionsForTesting;

namespace {

using TopicAdminCommand =
    std::function<void(::google::cloud::pubsub_admin::TopicAdminClient,
                       std::vector<std::string> const&)>;

// Delete all topics created with that include "cloud-cpp-samples". Ignore any
// failures. If multiple tests are cleaning up topics in parallel, then the
// delete call might fail.
void CleanupTopics(
    google::cloud::pubsub_admin::TopicAdminClient& topic_admin_client,
    std::string const& project_id, absl::Time time_now) {
  for (auto const& topic : topic_admin_client.ListTopics(
           google::cloud::Project(project_id).FullName())) {
    if (!topic) continue;
    auto topic_name = topic->name();
    std::string const keyword = "cloud-cpp-samples";
    if (!absl::StrContains(topic->name(), keyword)) continue;
    // Extract the date from the resource name which is in the format
    // `*-cloud-cpp-samples-YYYY-MM-DD-`.
    auto date =
        topic_name.substr(topic_name.find(keyword) + keyword.size() + 1, 10);

    absl::CivilDay day;
    if (absl::ParseCivilTime(date, &day) &&
        absl::FromCivil(day, absl::UTCTimeZone()) <
            time_now - absl::Hours(36)) {
      google::pubsub::v1::DeleteTopicRequest request;
      request.set_topic(topic->name());
      (void)topic_admin_client.DeleteTopic(request);
    }
  }
}

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
  // [START pubsub_quickstart_create_topic]
  // [START pubsub_create_topic]
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
  // [END pubsub_create_topic]
  // [END pubsub_quickstart_create_topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateTopicWithSchema(google::cloud::pubsub_admin::TopicAdminClient client,
                           std::vector<std::string> const& argv) {
  // [START pubsub_create_topic_with_schema]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string schema_id, std::string const& encoding) {
    google::pubsub::v1::Topic request;
    request.set_name(pubsub::Topic(project_id, std::move(topic_id)).FullName());
    request.mutable_schema_settings()->set_schema(
        pubsub::Schema(std::move(project_id), std::move(schema_id)).FullName());
    request.mutable_schema_settings()->set_encoding(
        encoding == "JSON" ? google::pubsub::v1::JSON
                           : google::pubsub::v1::BINARY);
    auto topic = client.CreateTopic(request);

    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  // [END pubsub_create_topic_with_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateTopicWithSchemaRevisions(
    google::cloud::pubsub_admin::TopicAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_topic_with_schema_revisions]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string schema_id, std::string const& encoding,
     std::string const& first_revision_id,
     std::string const& last_revision_id) {
    google::pubsub::v1::Topic request;
    request.set_name(pubsub::Topic(project_id, std::move(topic_id)).FullName());
    request.mutable_schema_settings()->set_schema(
        pubsub::Schema(std::move(project_id), std::move(schema_id)).FullName());
    request.mutable_schema_settings()->set_encoding(
        encoding == "JSON" ? google::pubsub::v1::JSON
                           : google::pubsub::v1::BINARY);
    request.mutable_schema_settings()->set_first_revision_id(first_revision_id);
    request.mutable_schema_settings()->set_last_revision_id(last_revision_id);
    auto topic = client.CreateTopic(request);

    // Note that kAlreadyExists is a possible error when the
    // library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  // [END pubsub_create_topic_with_schema_revisions]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4), argv.at(5));
}

void CreateTopicWithKinesisIngestion(
    google::cloud::pubsub_admin::TopicAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_topic_with_kinesis_ingestion]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string stream_arn, std::string consumer_arn,
     std::string aws_role_arn, std::string gcp_service_account) {
    google::pubsub::v1::Topic request;
    request.set_name(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    auto* aws_kinesis =
        request.mutable_ingestion_data_source_settings()->mutable_aws_kinesis();
    aws_kinesis->set_stream_arn(stream_arn);
    aws_kinesis->set_consumer_arn(consumer_arn);
    aws_kinesis->set_aws_role_arn(aws_role_arn);
    aws_kinesis->set_gcp_service_account(gcp_service_account);

    auto topic = client.CreateTopic(request);
    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  // [END pubsub_create_topic_with_kinesis_ingestion]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4), argv.at(5));
}

void CreateTopicWithCloudStorageIngestion(
    google::cloud::pubsub_admin::TopicAdminClient client,
    std::vector<std::string> const& argv) {
  //! [pubsub_create_topic_with_cloud_storage_ingestion]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string bucket, std::string input_format,
     std::string text_delimiter, std::string match_glob,
     std::string minimum_object_create_time) {
    google::pubsub::v1::Topic request;
    request.set_name(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    auto* cloud_storage = request.mutable_ingestion_data_source_settings()
                              ->mutable_cloud_storage();
    cloud_storage->set_bucket(bucket);
    if (input_format == "text") {
      cloud_storage->mutable_text_format()->set_delimiter(text_delimiter);
    } else if (input_format == "avro") {
      cloud_storage->mutable_avro_format();
    } else if (input_format == "pubsub_avro") {
      cloud_storage->mutable_pubsub_avro_format();
    } else {
      std::cout << "input_format must be in ('text', 'avro', 'pubsub_avro'); "
                   "got value: "
                << input_format << std::endl;
      return;
    }

    if (!match_glob.empty()) {
      cloud_storage->set_match_glob(match_glob);
    }

    if (!minimum_object_create_time.empty()) {
      google::protobuf::Timestamp timestamp;
      if (!google::protobuf::util::TimeUtil::FromString(
              minimum_object_create_time,
              cloud_storage->mutable_minimum_object_create_time())) {
        std::cout << "Invalid minimum object create time: "
                  << minimum_object_create_time << std::endl;
      }
    }

    auto topic = client.CreateTopic(request);
    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [pubsub_create_topic_with_cloud_storage_ingestion]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4), argv.at(5), argv.at(6));
}

void GetTopic(google::cloud::pubsub_admin::TopicAdminClient client,
              std::vector<std::string> const& argv) {
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.GetTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic information was successfully retrieved: "
              << topic->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void UpdateTopic(google::cloud::pubsub_admin::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
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
  }(std::move(client), argv.at(0), argv.at(1));
}

void UpdateTopicSchema(google::cloud::pubsub_admin::TopicAdminClient client,
                       std::vector<std::string> const& argv) {
  // [START pubsub_update_topic_schema]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string const& first_revision_id,
     std::string const& last_revision_id) {
    google::pubsub::v1::UpdateTopicRequest request;
    auto* request_topic = request.mutable_topic();
    request_topic->set_name(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    request_topic->mutable_schema_settings()->set_first_revision_id(
        first_revision_id);
    request_topic->mutable_schema_settings()->set_last_revision_id(
        last_revision_id);
    *request.mutable_update_mask()->add_paths() =
        "schema_settings.first_revision_id";
    *request.mutable_update_mask()->add_paths() =
        "schema_settings.last_revision_id";
    auto topic = client.UpdateTopic(request);

    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully updated: " << topic->DebugString()
              << "\n";
  }
  // [END pubsub_update_topic_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void UpdateTopicType(google::cloud::pubsub_admin::TopicAdminClient client,
                     std::vector<std::string> const& argv) {
  // [START pubsub_update_topic_type]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string stream_arn, std::string consumer_arn,
     std::string aws_role_arn, std::string gcp_service_account) {
    google::pubsub::v1::UpdateTopicRequest request;

    request.mutable_topic()->set_name(
        pubsub::Topic(std::move(project_id), std::move(topic_id)).FullName());
    auto* aws_kinesis = request.mutable_topic()
                            ->mutable_ingestion_data_source_settings()
                            ->mutable_aws_kinesis();
    aws_kinesis->set_stream_arn(stream_arn);
    aws_kinesis->set_consumer_arn(consumer_arn);
    aws_kinesis->set_aws_role_arn(aws_role_arn);
    aws_kinesis->set_gcp_service_account(gcp_service_account);
    *request.mutable_update_mask()->add_paths() =
        "ingestion_data_source_settings";

    auto topic = client.UpdateTopic(request);
    if (!topic) throw std::move(topic).status();

    std::cout << "The topic was successfully updated: " << topic->DebugString()
              << "\n";
  }
  // [END pubsub_update_topic_type]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4), argv.at(5));
}

void ListTopics(google::cloud::pubsub_admin::TopicAdminClient client,
                std::vector<std::string> const& argv) {
  // [START pubsub_list_topics]
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
  // [END pubsub_list_topics]
  (std::move(client), argv.at(0));
}

void ListTopicSubscriptions(
    google::cloud::pubsub_admin::TopicAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_list_topic_subscriptions]
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
  // [END pubsub_list_topic_subscriptions]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopicSnapshots(google::cloud::pubsub_admin::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
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
  }(std::move(client), argv.at(0), argv.at(1));
}

void DetachSubscription(google::cloud::pubsub_admin::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
  // [START pubsub_detach_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    auto response = client.DetachSubscription(request);
    if (!response.ok()) throw std::move(response).status();

    std::cout << "The subscription was successfully detached: "
              << response->DebugString() << "\n";
  }
  // [END pubsub_detach_subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteTopic(google::cloud::pubsub_admin::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  // [START pubsub_delete_topic]
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
  // [END pubsub_delete_topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void AutoRunAvro(
    std::string const& project_id, std::string const& topic_id,
    std::string const& schema_id, std::string const& testdata_directory,
    google::cloud::pubsub_admin::TopicAdminClient& topic_admin_client) {
  auto schema_admin = google::cloud::pubsub::SchemaServiceClient(
      google::cloud::pubsub::MakeSchemaServiceConnection());

  // The following commands require a schema for testing. This creates a schema
  // with multiple revisions.
  auto avro_revision_schema_id = "avro-revision-" + schema_id;
  auto const avro_revision_topic_id = "avro-revision-" + topic_id;
  auto const revision_ids = CommitSchemaWithRevisionsForTesting(
      schema_admin, project_id, avro_revision_schema_id,
      testdata_directory + "schema.avsc",
      testdata_directory + "revised_schema.avsc", "AVRO");
  auto const first_revision_id = revision_ids.first;
  auto const last_revision_id = revision_ids.second;
  Cleanup cleanup;
  cleanup.Defer([schema_admin, project_id, avro_revision_schema_id]() mutable {
    std::cout << "\nDelete revision topic " + avro_revision_schema_id +
                     " [avro]"
              << std::endl;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(
        google::cloud::pubsub::Schema(project_id, avro_revision_schema_id)
            .FullName());
    schema_admin.DeleteSchema(request);
  });

  std::cout << "\nRunning CreateTopicWithSchemaRevisions sample [avro]"
            << std::endl;
  CreateTopicWithSchemaRevisions(
      topic_admin_client,
      {project_id, avro_revision_topic_id, avro_revision_schema_id, "JSON",
       first_revision_id, last_revision_id});
  cleanup.Defer(
      [topic_admin_client, project_id, avro_revision_topic_id]() mutable {
        std::cout << "\nDelete topic " + avro_revision_topic_id + " [avro]"
                  << std::endl;
        DeleteTopic(topic_admin_client, {project_id, avro_revision_topic_id});
      });

  std::cout << "\nRunning UpdateTopicSchema sample [avro]" << std::endl;
  UpdateTopicSchema(topic_admin_client, {project_id, avro_revision_topic_id,
                                         first_revision_id, first_revision_id});

  // Re-use the schema from before.
  std::cout << "\nRunning CreateTopicWithSchema() sample [avro]" << std::endl;
  auto const avro_topic_id = "avro-" + topic_id;
  CreateTopicWithSchema(topic_admin_client, {project_id, avro_topic_id,
                                             avro_revision_schema_id, "JSON"});
  std::cout << "\nCreate topic (" << avro_topic_id << ")" << std::endl;
  cleanup.Defer([topic_admin_client, project_id, avro_topic_id]() mutable {
    std::cout << "\nDelete revision topic " + avro_topic_id + " [avro]"
              << std::endl;
    DeleteTopic(topic_admin_client, {project_id, avro_topic_id});
  });
}

void AutoRunProtobuf(
    std::string const& project_id, std::string const& topic_id,
    std::string const& schema_id, std::string const& testdata_directory,
    google::cloud::pubsub_admin::TopicAdminClient& topic_admin_client) {
  auto schema_admin = google::cloud::pubsub::SchemaServiceClient(
      google::cloud::pubsub::MakeSchemaServiceConnection());

  // The following commands require a schema for testing. This creates a schema
  // with multiple revisions.
  auto proto_revision_schema_id = "proto-revision-" + schema_id;
  auto const proto_revision_topic_id = "proto-revision-" + topic_id;
  auto const revision_ids = CommitSchemaWithRevisionsForTesting(
      schema_admin, project_id, proto_revision_schema_id,
      testdata_directory + "schema.proto",
      testdata_directory + "revised_schema.proto", "PROTO");
  auto const first_revision_id = revision_ids.first;
  auto const last_revision_id = revision_ids.second;

  Cleanup cleanup;
  cleanup.Defer([schema_admin, project_id, proto_revision_schema_id]() mutable {
    std::cout << "\nDelete revision topic " + proto_revision_schema_id +
                     " [proto]"
              << std::endl;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(
        google::cloud::pubsub::Schema(project_id, proto_revision_schema_id)
            .FullName());
    schema_admin.DeleteSchema(request);
  });

  std::cout << "\nRunning CreateTopicWithSchemaRevisions sample [proto]"
            << std::endl;
  CreateTopicWithSchemaRevisions(
      topic_admin_client,
      {project_id, proto_revision_topic_id, proto_revision_schema_id, "BINARY",
       first_revision_id, last_revision_id});
  cleanup.Defer(
      [topic_admin_client, project_id, proto_revision_topic_id]() mutable {
        std::cout << "\nDelete topic " + proto_revision_topic_id + " [proto]"
                  << std::endl;
        DeleteTopic(topic_admin_client, {project_id, proto_revision_topic_id});
      });

  std::cout << "\nRunning UpdateTopicSchema sample [proto]" << std::endl;
  UpdateTopicSchema(topic_admin_client, {project_id, proto_revision_topic_id,
                                         first_revision_id, first_revision_id});

  // Re-use the schema from before.
  std::cout << "\nRunning CreateTopicWithSchema() sample [proto]" << std::endl;
  auto const proto_topic_id = "proto-" + topic_id;
  CreateTopicWithSchema(
      topic_admin_client,
      {project_id, proto_topic_id, proto_revision_schema_id, "BINARY"});
  std::cout << "\nCreate topic (" << proto_topic_id << ")" << std::endl;
  cleanup.Defer([topic_admin_client, project_id, proto_topic_id]() mutable {
    std::cout << "\nDelete revision topic " + proto_topic_id + " [proto]"
              << std::endl;
    DeleteTopic(topic_admin_client, {project_id, proto_topic_id});
  });
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::pubsub::examples::RandomSchemaId;
  using ::google::cloud::pubsub::examples::RandomSubscriptionId;
  using ::google::cloud::pubsub::examples::RandomTopicId;
  using ::google::cloud::pubsub::examples::UsingEmulator;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  // For CMake builds, use the environment variable. For Bazel builds, use the
  // relative path to the file.
  auto const testdata_directory =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_PUBSUB_TESTDATA")
          .value_or("./google/cloud/pubsub/samples/testdata/");

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const topic = google::cloud::pubsub::Topic(project_id, topic_id);
  auto const subscription_id = RandomSubscriptionId(generator);
  auto const subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto const schema_topic_id = RandomTopicId(generator);
  auto const schema_id = RandomSchemaId(generator);
  auto const kinesis_topic_id =
      "kinesis-" + RandomTopicId(generator) + "_ingestion_topic";
  auto const* const kinesis_stream_arn =
      "arn:aws:kinesis:us-west-2:111111111111:stream/fake-stream-name";
  auto const* const kinesis_consumer_arn =
      "arn:aws:kinesis:us-west-2:111111111111:stream/fake-stream-name/consumer/"
      "consumer-1:1111111111";
  auto const* const kinesis_aws_role_arn =
      "arn:aws:iam::111111111111:role/fake-role-name";
  auto const* const kinesis_gcp_service_account =
      "fake-service-account@fake-gcp-project.iam.gserviceaccount.com";
  auto const* const kinesis_updated_gcp_service_account =
      "fake-update-service-account@fake-gcp-project.iam.gserviceaccount.com";
  auto const cloud_storage_topic_id =
      "cloud-storage-" + RandomTopicId(generator) + "_ingestion_topic";
  auto const cloud_storage_bucket = project_id + "-pubsub-bucket";

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
  google::cloud::pubsub_admin::SubscriptionAdminClient
      subscription_admin_client(
          google::cloud::pubsub_admin::MakeSubscriptionAdminConnection());

  // Delete resources over 3 days old.
  std::cout << "Cleaning up old topics...\n";
  CleanupTopics(topic_admin_client, project_id,
                absl::FromChrono(std::chrono::system_clock::now()));

  std::cout << "\nRunning CreateTopic() sample [1]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});
  std::cout << "\nCreate topic (" << topic_id << ")" << std::endl;
  Cleanup cleanup;
  cleanup.Defer([topic_admin_client, project_id, topic_id]() mutable {
    std::cout << "\nRunning DeleteTopic() sample" << std::endl;
    DeleteTopic(topic_admin_client, {project_id, topic_id});
  });

  // Since the topic was created already, this should return kAlreadyExists.
  std::cout << "\nRunning CreateTopic() sample [2]" << std::endl;

  std::cout << "\nRunning CreateTopicWithKinesisIngestion() sample"
            << std::endl;

  CreateTopicWithKinesisIngestion(
      topic_admin_client,
      {project_id, kinesis_topic_id, kinesis_stream_arn, kinesis_consumer_arn,
       kinesis_aws_role_arn, kinesis_gcp_service_account});
  cleanup.Defer([topic_admin_client, project_id, kinesis_topic_id]() mutable {
    std::cout << "\nRunning DeleteTopic() sample" << std::endl;
    DeleteTopic(topic_admin_client, {project_id, kinesis_topic_id});
  });

  std::cout << "\nRunning CreateTopicWithCloudStorage() sample"
            << std::endl;

  ignore_emulator_failures(
      [&] {
        CreateTopicWithCloudStorageIngestion(
            topic_admin_client,
            {project_id, cloud_storage_topic_id, "mikeprieto-bucket", "text",
             "\n", "**.txt", "2024-09-26T00:00:00Z"});
        std::cout << "\nAfter Running CreateTopicWithCloudStorage() sample"
                  << std::endl;
        cleanup.Defer([topic_admin_client, project_id,
                       cloud_storage_topic_id]() mutable {
          std::cout << "\nRunning DeleteTopic() sample" << std::endl;
          DeleteTopic(topic_admin_client, {project_id, cloud_storage_topic_id});
        });
      },
      StatusCode::kInvalidArgument);

  std::cout << "\nRunning UpdateTopicType() sample" << std::endl;

  UpdateTopicType(
      topic_admin_client,
      {project_id, kinesis_topic_id, kinesis_stream_arn, kinesis_consumer_arn,
       kinesis_aws_role_arn, kinesis_updated_gcp_service_account});

  std::cout << "\nRunning GetTopic() sample" << std::endl;
  GetTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning UpdateTopic() sample" << std::endl;
  ignore_emulator_failures(
      [&] { UpdateTopic(topic_admin_client, {project_id, topic_id}); },
      StatusCode::kInvalidArgument);

  std::cout << "\nRunning ListTopics() sample" << std::endl;
  ListTopics(topic_admin_client, {project_id});

  std::cout << "\nRunning ListTopicSnapshots() sample" << std::endl;
  ListTopicSnapshots(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning ListTopicSubscriptions() sample" << std::endl;
  ListTopicSubscriptions(topic_admin_client, {project_id, topic_id});

  std::cout << "\nCreate subscription (" << subscription_id << ")" << std::endl;
  google::pubsub::v1::Subscription request;
  request.set_name(
      google::cloud::pubsub::Subscription(project_id, subscription_id)
          .FullName());
  request.set_topic(
      google::cloud::pubsub::Topic(project_id, topic_id).FullName());
  subscription_admin_client.CreateSubscription(request);
  cleanup.Defer([subscription_admin_client, subscription]() mutable {
    std::cout << "\nDelete subscription (" << subscription.subscription_id()
              << ")" << std::endl;
    (void)subscription_admin_client.DeleteSubscription(subscription.FullName());
  });

  std::cout << "\nRunning DetachSubscription() sample" << std::endl;
  ignore_emulator_failures([&] {
    DetachSubscription(topic_admin_client, {project_id, subscription_id});
  });

  ignore_emulator_failures([&] {
    AutoRunAvro(project_id, schema_topic_id, schema_id, testdata_directory,
                topic_admin_client);
  });
  ignore_emulator_failures([&] {
    AutoRunProtobuf(project_id, schema_topic_id, schema_id, testdata_directory,
                    topic_admin_client);
  });
  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateTopicAdminCommand("create-topic", {"project-id", "topic-id"},
                              CreateTopic),
      CreateTopicAdminCommand(
          "create-topic-with-kinesis-ingestion",
          {"project-id", "topic-id", "stream-arn", "consumer-arn",
           "aws-role-arn", "gcp-service-account"},
          CreateTopicWithKinesisIngestion),
      CreateTopicAdminCommand(
          "create-topic-with-cloud-storage-ingestion",
          {"project-id", "topic-id", "bucket", "input-format", "text-delimiter",
           "match-glob", "minimum-object-create-time"},
          CreateTopicWithCloudStorageIngestion),
      CreateTopicAdminCommand(
          "create-topic-with-schema",
          {"project-id", "topic-id", "schema-id", "encoding"},
          CreateTopicWithSchema),
      CreateTopicAdminCommand(
          "create-topic-with-schema-revisions",
          {"project-id", "topic-id", "schema-id", "encoding",
           "first-revision-id", "last-revision-id"},
          CreateTopicWithSchemaRevisions),
      CreateTopicAdminCommand("get-topic", {"project-id", "topic-id"},
                              GetTopic),
      CreateTopicAdminCommand("update-topic", {"project-id", "topic-id"},
                              UpdateTopic),
      CreateTopicAdminCommand(
          "update-topic-schema",
          {"project-id", "topic-id", "first-revision-id", "last-revision-id"},
          UpdateTopicSchema),
      CreateTopicAdminCommand(
          "update-topic-type",
          {"project-id", "topic-id", "stream-arn", "consumer-arn",
           "aws-role-arn", "gcp-service-account"},
          UpdateTopicType),
      CreateTopicAdminCommand("list-topics", {"project-id"}, ListTopics),
      CreateTopicAdminCommand("list-topic-subscriptions",
                              {"project-id", "topic-id"},
                              ListTopicSubscriptions),
      CreateTopicAdminCommand("list-topic-snapshots",
                              {"project-id", "topic-id"}, ListTopicSnapshots),
      CreateTopicAdminCommand("detach-subscription",
                              {"project-id", "subscription-id"},
                              DetachSubscription),
      CreateTopicAdminCommand("delete-topic", {"project-id", "topic-id"},
                              DeleteTopic),
      {"auto", AutoRun},
  });

  return example.Run(argc, argv);
}
