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

#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/schema_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/example_driver.h"
#include <chrono>
#include <iostream>
#include <string>

namespace {

using ::google::cloud::pubsub::examples::CleanupSchemas;
using ::google::cloud::pubsub::examples::CommitSchemaWithRevisionsForTesting;
using ::google::cloud::pubsub::examples::RandomSchemaId;
using ::google::cloud::pubsub::examples::ReadFile;
using ::google::cloud::pubsub::examples::UsingEmulator;

void CreateAvroSchema(google::cloud::pubsub::SchemaServiceClient client,
                      std::vector<std::string> const& argv) {
  //! [START pubsub_create_avro_schema] [create-avro-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::CreateSchemaRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.set_schema_id(schema_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.CreateSchema(request);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema already exists\n";
      return;
    }
    if (!schema) throw std::move(schema).status();

    std::cout << "Schema successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [END pubsub_create_avro_schema] [create-avro-schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateProtobufSchema(google::cloud::pubsub::SchemaServiceClient client,
                          std::vector<std::string> const& argv) {
  //! [START pubsub_create_proto_schema] [create-protobuf-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::CreateSchemaRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.set_schema_id(schema_id);
    request.mutable_schema()->set_type(
        google::pubsub::v1::Schema::PROTOCOL_BUFFER);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.CreateSchema(request);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema already exists\n";
      return;
    }
    if (!schema) throw std::move(schema).status();
    std::cout << "Schema successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [END pubsub_create_proto_schema] [create-protobuf-schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CommitAvroSchema(google::cloud::pubsub::SchemaServiceClient client,
                      std::vector<std::string> const& argv) {
  //! [START pubsub_commit_avro_schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::CommitSchemaRequest request;
    std::string const name =
        google::cloud::pubsub::Schema(project_id, schema_id).FullName();
    request.set_name(name);
    request.mutable_schema()->set_name(name);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.CommitSchema(request);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema revision already exists\n";
      return;
    }
    if (!schema) throw std::move(schema).status();

    std::cout << "Schema revision successfully committed: "
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_commit_avro_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CommitProtobufSchema(google::cloud::pubsub::SchemaServiceClient client,
                          std::vector<std::string> const& argv) {
  //! [START pubsub_commit_proto_schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::CommitSchemaRequest request;
    std::string const name =
        google::cloud::pubsub::Schema(project_id, schema_id).FullName();
    request.set_name(name);
    request.mutable_schema()->set_name(name);
    request.mutable_schema()->set_type(
        google::pubsub::v1::Schema::PROTOCOL_BUFFER);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.CommitSchema(request);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema revision already exists\n";
      return;
    }
    if (!schema) throw std::move(schema).status();

    std::cout << "Schema revision successfully committed: "
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_commit_proto_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetSchema(google::cloud::pubsub::SchemaServiceClient client,
               std::vector<std::string> const& argv) {
  //! [START pubsub_get_schema] [get-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id) {
    google::pubsub::v1::GetSchemaRequest request;
    request.set_name(pubsub::Schema(project_id, schema_id).FullName());
    request.set_view(google::pubsub::v1::FULL);
    auto schema = client.GetSchema(request);
    if (!schema) throw std::move(schema).status();

    std::cout << "The schema exists and its metadata is:"
              << "\n"
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_get_schema] [get-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetSchemaRevision(google::cloud::pubsub::SchemaServiceClient client,
                       std::vector<std::string> const& argv) {
  //! [START pubsub_get_schema_revision]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& revision_id) {
    std::string const schema_id_with_revision = schema_id + "@" + revision_id;

    google::pubsub::v1::GetSchemaRequest request;
    request.set_name(
        pubsub::Schema(project_id, schema_id_with_revision).FullName());
    request.set_view(google::pubsub::v1::FULL);
    auto schema = client.GetSchema(request);
    if (!schema) throw std::move(schema).status();

    std::cout << "The schema revision exists and its metadata is:"
              << "\n"
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_get_schema_revision]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ListSchemas(google::cloud::pubsub::SchemaServiceClient client,
                 std::vector<std::string> const& argv) {
  //! [START pubsub_list_schemas] [list-schemas]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id) {
    auto const parent = google::cloud::Project(project_id).FullName();
    for (auto& s : client.ListSchemas(parent)) {
      if (!s) throw std::move(s).status();
      std::cout << "Schema: " << s->DebugString() << "\n";
    }
  }
  //! [END pubsub_list_schemas] [list-schemas]
  (std::move(client), argv.at(0));
}

void ListSchemaRevisions(google::cloud::pubsub::SchemaServiceClient client,
                         std::vector<std::string> const& argv) {
  //! [START pubsub_list_schema_revisions]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id) {
    auto const parent = pubsub::Schema(project_id, schema_id).FullName();
    for (auto& s : client.ListSchemaRevisions(parent)) {
      if (!s) throw std::move(s).status();
      std::cout << "Schema revision: " << s->DebugString() << "\n";
    }
  }
  //! [END pubsub_list_schema_revisions]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteSchema(google::cloud::pubsub::SchemaServiceClient client,
                  std::vector<std::string> const& argv) {
  //! [START pubsub_delete_schema] [delete-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id) {
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(pubsub::Schema(project_id, schema_id).FullName());
    auto status = client.DeleteSchema(request);
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The schema was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Schema successfully deleted\n";
  }
  //! [END pubsub_delete_schema] [delete-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteSchemaRevision(google::cloud::pubsub::SchemaServiceClient client,
                          std::vector<std::string> const& argv) {
  //! [START pubsub_delete_schema_revision]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& revision_id) {
    std::string const schema_id_with_revision = schema_id + "@" + revision_id;

    google::pubsub::v1::DeleteSchemaRevisionRequest request;
    request.set_name(
        pubsub::Schema(project_id, schema_id_with_revision).FullName());
    auto schema = client.DeleteSchemaRevision(request);
    if (!schema) throw std::move(schema).status();

    std::cout << "Deleted schema. Its metadata is:"
              << "\n"
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_delete_schema_revision]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void RollbackSchema(google::cloud::pubsub::SchemaServiceClient client,
                    std::vector<std::string> const& argv) {
  //! [START pubsub_rollback_schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& revision_id) {
    google::pubsub::v1::RollbackSchemaRequest request;
    request.set_name(pubsub::Schema(project_id, schema_id).FullName());
    request.set_revision_id(revision_id);
    auto schema = client.RollbackSchema(request);
    if (!schema) throw std::move(schema).status();

    std::cout << "Rolledback schema. Created a new schema and its metadata is:"
              << "\n"
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_rollback_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ValidateAvroSchema(google::cloud::pubsub::SchemaServiceClient client,
                        std::vector<std::string> const& argv) {
  //! [validate-avro-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::ValidateSchemaRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.ValidateSchema(request);
    if (!schema) throw std::move(schema).status();
    std::cout << "Schema is valid\n";
  }
  //! [validate-avro-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void ValidateProtobufSchema(google::cloud::pubsub::SchemaServiceClient client,
                            std::vector<std::string> const& argv) {
  //! [validate-protobuf-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_definition_file) {
    std::string const definition = ReadFile(schema_definition_file);

    google::pubsub::v1::ValidateSchemaRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.mutable_schema()->set_type(
        google::pubsub::v1::Schema::PROTOCOL_BUFFER);
    request.mutable_schema()->set_definition(definition);
    auto schema = client.ValidateSchema(request);
    if (!schema) throw std::move(schema).status();
    std::cout << "Schema is valid\n";
  }
  //! [validate-protobuf-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void ValidateMessageAvro(google::cloud::pubsub::SchemaServiceClient client,
                         std::vector<std::string> const& argv) {
  //! [validate-message-avro]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_definition_file,
     std::string const& message_file) {
    std::string const definition = ReadFile(schema_definition_file);
    auto message = ReadFile(message_file);

    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(definition);
    request.set_message(message);
    request.set_encoding(google::pubsub::v1::JSON);
    auto response = client.ValidateMessage(request);
    if (!response) throw std::move(response).status();
    std::cout << "Message is valid\n";
  }
  //! [validate-message-avro]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ValidateMessageProtobuf(google::cloud::pubsub::SchemaServiceClient client,
                             std::vector<std::string> const& argv) {
  //! [validate-message-protobuf]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_definition_file,
     std::string const& message_file) {
    std::string const definition = ReadFile(schema_definition_file);
    std::string const message = ReadFile(message_file);

    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.mutable_schema()->set_type(
        google::pubsub::v1::Schema::PROTOCOL_BUFFER);
    request.mutable_schema()->set_definition(definition);
    request.set_message(message);
    request.set_encoding(google::pubsub::v1::BINARY);
    auto response = client.ValidateMessage(request);
    if (!response) throw std::move(response).status();
    std::cout << "Message is valid\n";
  }
  //! [validate-message-protobuf]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ValidateMessageNamedSchema(
    google::cloud::pubsub::SchemaServiceClient client,
    std::vector<std::string> const& argv) {
  //! [validate-message-named-schema]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::SchemaServiceClient client, std::string const& project_id,
     std::string const& schema_id, std::string const& message_file) {
    std::string message = ReadFile(message_file);

    auto const schema = pubsub::Schema(project_id, schema_id);
    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent(google::cloud::Project(project_id).FullName());
    request.set_name(schema.FullName());
    request.set_message(message);
    request.set_encoding(google::pubsub::v1::BINARY);
    auto response = client.ValidateMessage(request);
    if (!response) throw std::move(response).status();
    std::cout << "Message is valid for schema " << schema << "\n";
  }
  //! [validate-message-named-schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void AutoRunAvro(std::string const& project_id,
                 std::string const& testdata_directory,
                 google::cloud::internal::DefaultPRNG& generator,
                 google::cloud::pubsub::SchemaServiceClient& schema_admin) {
  auto avro_schema_id = RandomSchemaId(generator);
  auto avro_schema_definition_file = testdata_directory + "schema.avsc";
  auto avro_revised_schema_definition_file =
      testdata_directory + "revised_schema.avsc";
  auto avro_message_file = testdata_directory + "valid_message.avsc";

  std::cout << "\nRunning CreateAvroSchema() sample" << std::endl;
  CreateAvroSchema(schema_admin,
                   {project_id, avro_schema_id, avro_schema_definition_file});

  std::cout << "\nRunning CommitAvroSchema() sample" << std::endl;
  CommitAvroSchema(schema_admin, {project_id, avro_schema_id,
                                  avro_revised_schema_definition_file});

  std::cout << "\nRunning ValidateAvroSchema() sample" << std::endl;
  ValidateAvroSchema(schema_admin, {project_id, avro_schema_definition_file});

  std::cout << "\nRunning ValidateMessageAvro() sample" << std::endl;
  ValidateMessageAvro(schema_admin, {project_id, avro_schema_definition_file,
                                     avro_message_file});

  std::cout << "\nRunning GetSchema sample" << std::endl;
  GetSchema(schema_admin, {project_id, avro_schema_id});

  // For testing commands that require a revision id.
  auto avro_revision_schema_id = RandomSchemaId(generator);
  auto const revision_ids = CommitSchemaWithRevisionsForTesting(
      schema_admin, project_id, avro_revision_schema_id,
      avro_schema_definition_file, avro_revised_schema_definition_file, "AVRO");
  auto const first_revision_id = revision_ids.first;
  auto const last_revision_id = revision_ids.second;

  std::cout << "\nRunning GetSchemaRevision sample" << std::endl;
  GetSchemaRevision(schema_admin,
                    {project_id, avro_revision_schema_id, first_revision_id});

  std::cout << "\nRunning RollbackSchema sample" << std::endl;
  RollbackSchema(schema_admin,
                 {project_id, avro_revision_schema_id, first_revision_id});

  std::cout << "\nRunning ListSchemaRevisions() sample" << std::endl;
  ListSchemaRevisions(schema_admin, {project_id, avro_schema_id});

  std::cout << "\nRunning DeleteSchemaRevision sample" << std::endl;
  DeleteSchemaRevision(schema_admin,
                       {project_id, avro_revision_schema_id, last_revision_id});

  DeleteSchema(schema_admin, {project_id, avro_revision_schema_id});
}

void AutoRunProtobuf(std::string const& project_id,
                     std::string const& testdata_directory,
                     google::cloud::internal::DefaultPRNG& generator,
                     google::cloud::pubsub::SchemaServiceClient& schema_admin) {
  auto proto_schema_id = RandomSchemaId(generator);
  auto proto_schema_definition_file = testdata_directory + "schema.proto";
  auto proto_revised_schema_definition_file =
      testdata_directory + "revised_schema.proto";
  auto proto_message_file = testdata_directory + "valid_message.pb";

  std::cout << "\nRunning CreateProtobufSchema() sample" << std::endl;
  CreateProtobufSchema(schema_admin, {project_id, proto_schema_id,
                                      proto_schema_definition_file});

  std::cout << "\nRunning CommitProtobufSchema() sample" << std::endl;
  CommitProtobufSchema(schema_admin, {project_id, proto_schema_id,
                                      proto_revised_schema_definition_file});

  std::cout << "\nRunning ValidateProtobufSchema() sample" << std::endl;
  ValidateProtobufSchema(schema_admin,
                         {project_id, proto_schema_definition_file});

  std::cout << "\nRunning ValidateMessageProtobuf() sample" << std::endl;
  ValidateMessageProtobuf(
      schema_admin,
      {project_id, proto_schema_definition_file, proto_message_file});

  std::cout << "\nRunning ValidateMessageNamedSchema() sample" << std::endl;
  ValidateMessageNamedSchema(schema_admin,
                             {project_id, proto_schema_id, proto_message_file});

  std::cout << "\nRunning DeleteSchema() sample [proto]" << std::endl;
  DeleteSchema(schema_admin, {project_id, proto_schema_id});
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

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
  auto schema_admin = google::cloud::pubsub::SchemaServiceClient(
      google::cloud::pubsub::MakeSchemaServiceConnection());

  CleanupSchemas(schema_admin, project_id,
                 absl::FromChrono(std::chrono::system_clock::now()));

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

  std::cout << "\nRunning ListSchemas() sample" << std::endl;
  ListSchemas(schema_admin, {project_id});

  ignore_emulator_failures([&] {
    AutoRunAvro(project_id, testdata_directory, generator, schema_admin);
  });
  ignore_emulator_failures([&] {
    AutoRunProtobuf(project_id, testdata_directory, generator, schema_admin);
  });

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::pubsub::examples::CreateSchemaServiceCommand;
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateSchemaServiceCommand(
          "create-avro-schema",
          {"project-id", "schema-id", "schema-definition-file"},
          CreateAvroSchema),
      CreateSchemaServiceCommand(
          "create-protobuf-schema",
          {"project-id", "schema-id", "schema-definition-file"},
          CreateProtobufSchema),
      CreateSchemaServiceCommand(
          "commit-avro-schema",
          {"project-id", "schema-id", "schema-definition-file"},
          CommitAvroSchema),
      CreateSchemaServiceCommand(
          "commit-protobuf-schema",
          {"project-id", "schema-id", "schema-definition-file"},
          CommitProtobufSchema),
      CreateSchemaServiceCommand("get-schema", {"project-id", "schema-id"},
                                 GetSchema),
      CreateSchemaServiceCommand("get-schema-revision",
                                 {"project-id", "schema-id", "revision-id"},
                                 GetSchemaRevision),
      CreateSchemaServiceCommand("list-schemas", {"project-id"}, ListSchemas),
      CreateSchemaServiceCommand("list-schema-revisions",
                                 {"project-id", "schema-id"},
                                 ListSchemaRevisions),
      CreateSchemaServiceCommand("delete-schema", {"project-id", "schema-id"},
                                 DeleteSchema),
      CreateSchemaServiceCommand("delete-schema-revision",
                                 {"project-id", "schema-id", "revision-id"},
                                 DeleteSchemaRevision),
      CreateSchemaServiceCommand("rollback-schema",
                                 {"project-id", "schema-id", "revision-id"},
                                 RollbackSchema),
      CreateSchemaServiceCommand("validate-avro-schema",
                                 {"project-id", "schema-definition-file"},
                                 ValidateAvroSchema),
      CreateSchemaServiceCommand("validate-protobuf-schema",
                                 {"project-id", "schema-definition-file"},
                                 ValidateProtobufSchema),
      CreateSchemaServiceCommand(
          "validate-message-avro",
          {"project-id", "schema-definition-file", "message-file"},
          ValidateMessageAvro),
      CreateSchemaServiceCommand(
          "validate-message-protobuf",
          {"project-id", "schema-definition-file", "message-file"},
          ValidateMessageProtobuf),
      CreateSchemaServiceCommand("validate-message-named-schema",
                                 {"project-id", "schema-id", "message-file"},
                                 ValidateMessageNamedSchema),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
