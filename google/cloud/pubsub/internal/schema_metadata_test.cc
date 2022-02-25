// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/schema_metadata.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/testing/mock_schema_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::Schema;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;

class SchemaMetadataTest : public ::testing::Test {
 protected:
  Status IsContextMDValid(grpc::ClientContext& context,
                          std::string const& method) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, google::cloud::internal::ApiClientHeader());
  }

  void ValidateNoUserProject(grpc::ClientContext& context) {
    auto md = validate_metadata_fixture_.GetMetadata(context);
    EXPECT_THAT(md, Not(Contains(Pair("x-goog-user-project", _))));
  }

  void ValidateTestUserProject(grpc::ClientContext& context) {
    auto md = validate_metadata_fixture_.GetMetadata(context);
    EXPECT_THAT(md, Contains(Pair("x-goog-user-project", "test-project")));
  }

  static Options TestOptions(std::string const& user_project) {
    return user_project.empty()
               ? Options{}
               : Options{}.set<UserProjectOption>("test-project");
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(SchemaMetadataTest, CreateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::CreateSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.CreateSchema"),
                    IsOk());
        return make_status_or(google::pubsub::v1::Schema{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::CreateSchemaRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Schema{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::CreateSchemaRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Schema{});
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::CreateSchemaRequest request;
    request.set_parent("projects/test-project");
    auto status = stub.CreateSchema(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

TEST_F(SchemaMetadataTest, GetSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, GetSchema)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.GetSchema"),
                    IsOk());
        return make_status_or(google::pubsub::v1::Schema{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSchemaRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::Schema{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::GetSchemaRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::Schema{});
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::GetSchemaRequest request;
    request.set_name(Schema("test-project", "test-schema").FullName());
    auto status = stub.GetSchema(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

TEST_F(SchemaMetadataTest, ListSchemas) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSchemasRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.ListSchemas"),
                    IsOk());
        return make_status_or(google::pubsub::v1::ListSchemasResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSchemasRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ListSchemasResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ListSchemasRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ListSchemasResponse{});
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ListSchemasRequest request;
    request.set_parent("projects/test-project");
    auto status = stub.ListSchemas(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

TEST_F(SchemaMetadataTest, DeleteSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, DeleteSchema)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.DeleteSchema"),
                    IsOk());
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSchemaRequest const&) {
        ValidateNoUserProject(context);
        return Status{};
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::DeleteSchemaRequest const&) {
        ValidateTestUserProject(context);
        return Status{};
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(Schema("test-project", "test-schema").FullName());
    auto status = stub.DeleteSchema(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

TEST_F(SchemaMetadataTest, ValidateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateSchemaRequest const&) {
        EXPECT_THAT(
            IsContextMDValid(context,
                             "google.pubsub.v1.SchemaService.ValidateSchema"),
            IsOk());
        return make_status_or(google::pubsub::v1::ValidateSchemaResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateSchemaRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ValidateSchemaResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateSchemaRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ValidateSchemaResponse{});
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ValidateSchemaRequest request;
    request.set_parent("projects/test-project");
    auto status = stub.ValidateSchema(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

TEST_F(SchemaMetadataTest, ValidateMessage) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateMessageRequest const&) {
        EXPECT_THAT(
            IsContextMDValid(context,
                             "google.pubsub.v1.SchemaService.ValidateMessage"),
            IsOk());
        return make_status_or(google::pubsub::v1::ValidateMessageResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateMessageRequest const&) {
        ValidateNoUserProject(context);
        return make_status_or(google::pubsub::v1::ValidateMessageResponse{});
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::pubsub::v1::ValidateMessageRequest const&) {
        ValidateTestUserProject(context);
        return make_status_or(google::pubsub::v1::ValidateMessageResponse{});
      });

  SchemaMetadata stub(mock);
  for (auto const* user_project : {"", "", "test-project"}) {
    internal::OptionsSpan span(TestOptions(user_project));
    grpc::ClientContext context;
    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent("projects/test-project");
    auto status = stub.ValidateMessage(context, request);
    EXPECT_THAT(status, IsOk());
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
