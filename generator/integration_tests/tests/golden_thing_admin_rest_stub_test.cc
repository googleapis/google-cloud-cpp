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

#include "generator/integration_tests/golden/internal/golden_thing_admin_rest_stub.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::testing::_;
using ::testing::A;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;

std::unique_ptr<MockRestResponse> CreateMockRestResponse(
    std::string const& json_response,
    HttpStatusCode http_status_code = HttpStatusCode::kOk) {
  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode()).WillOnce([=] {
    return http_status_code;
  });
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });
  return mock_response;
}

// This first test has a lot of overlap with the unit tests in
// rest_stub_helpers_test.cc just to make sure code generation works on both
// success and failure paths. Subsequent tests only check what the stub code
// affects and do not duplicate testing whether the HTTP helper methods work as
// they are tested elsewhere.
TEST(GoldenThingAdminRestStubTest, ListDatabases) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kServiceUnavailable = "503 Service Unavailable";
  std::string service_unavailable = kServiceUnavailable;
  auto constexpr kJsonResponsePayload = R"(
    {
      "databases":[{"name":"Tom"},{"name":"Dick"},{"name":"Harry"}],
      "next_page_token":"my_next_page_token"
    })";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;

  auto mock_503_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_503_response, StatusCode()).WillRepeatedly([]() {
    return HttpStatusCode::kServiceUnavailable;
  });
  EXPECT_CALL(std::move(*mock_503_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(service_unavailable);
  });

  google::test::admin::database::v1::ListDatabasesRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");
  proto_request.set_page_size(100);
  proto_request.set_page_token("my_page_token");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get)
      .WillOnce([&](RestRequest const&) {
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_503_response.release());
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(
            request.path(),
            Eq("/v1/projects/my_project/instances/my_instance/databases"));
        EXPECT_THAT(request.GetQueryParameter("page_size"), Contains("100"));
        EXPECT_THAT(request.GetQueryParameter("page_token"),
                    Contains("my_page_token"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto failure = stub.ListDatabases(rest_context, proto_request);
  EXPECT_EQ(failure.status(),
            Status(StatusCode::kUnavailable, kServiceUnavailable));
  auto success = stub.ListDatabases(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  std::vector<std::string> database_names;
  for (auto const& d : success->databases()) {
    database_names.push_back(d.name());
  }
  EXPECT_THAT(database_names, ElementsAre("Tom", "Dick", "Harry"));
  EXPECT_THAT(success->next_page_token(), Eq("my_next_page_token"));
}

TEST(GoldenThingAdminRestStubTest, CreateDatabase) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"my_operation","done":"true"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::CreateDatabaseRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(
            request.path(),
            Eq("/v1/projects/my_project/instances/my_instance/databases"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.CreateDatabase(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->name(), Eq("my_operation"));
  EXPECT_TRUE(success->done());
}

TEST(GoldenThingAdminRestStubTest, GetDatabase) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"projects/my_project/instances/my_instance/databases/my_database","state":2})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::GetDatabaseRequest proto_request;
  proto_request.set_name(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                   "my_instance/databases/my_database"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.GetDatabase(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(
      success->name(),
      Eq("projects/my_project/instances/my_instance/databases/my_database"));
  EXPECT_THAT(success->state(),
              Eq(google::test::admin::database::v1::Database_State_READY));
}

TEST(GoldenThingAdminRestStubTest, UpdateDatabaseDdl) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"my_operation","done":"true"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::UpdateDatabaseDdlRequest proto_request;
  proto_request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Patch)
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/"
                       "my_instance/databases/my_database/ddl"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.UpdateDatabaseDdl(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->name(), Eq("my_operation"));
  EXPECT_TRUE(success->done());
}

TEST(GoldenThingAdminRestStubTest, DropDatabase) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"projects/my_project/instances/my_instance/databases/my_database","state":2})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::DropDatabaseRequest proto_request;
  proto_request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Delete)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                       "my_instance/databases/my_database"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.DropDatabase(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

TEST(GoldenThingAdminRestStubTest, GetDatabaseDdl) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"statements":["create table foo", "create table bar"]})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::GetDatabaseDdlRequest proto_request;
  proto_request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                   "my_instance/databases/my_database/ddl"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.GetDatabaseDdl(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->statements(),
              ElementsAre("create table foo", "create table bar"));
}

auto constexpr kJsonIamPolicyResponsePayload = R"(
     {
       "bindings": [
         {
           "role": "roles/resourcemanager.organizationAdmin",
           "members": [
             "user:mike@example.com",
             "group:admins@example.com",
             "domain:google.com",
             "serviceAccount:my-project-id@appspot.gserviceaccount.com"
           ]
         },
         {
           "role": "roles/resourcemanager.organizationViewer",
           "members": [
             "user:eve@example.com"
           ],
           "condition": {
             "title": "expirable access",
             "description": "Does not grant access after Sep 2020",

           }
         }
       ],
       "etag": "BwWWja0YfJA=",
       "version": 3
     })";

TEST(GoldenThingAdminRestStubTest, SetIamPolicy) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  std::string json_response_database(kJsonIamPolicyResponsePayload);
  std::string json_response_backup(kJsonIamPolicyResponsePayload);
  RestContext rest_context;
  google::iam::v1::SetIamPolicyRequest proto_request;
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response_database =
      CreateMockRestResponse(json_response_database);
  auto mock_200_response_backup = CreateMockRestResponse(json_response_backup);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/"
                       "databases/my_database:setIamPolicy"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_database.release());
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/backups/"
                       "my_backup:setIamPolicy"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_backup.release());
      });

  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto database_success = stub.SetIamPolicy(rest_context, proto_request);
  ASSERT_THAT(database_success, IsOk());
  EXPECT_THAT(database_success->bindings(0).role(),
              Eq("roles/resourcemanager.organizationAdmin"));
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto backup_success = stub.SetIamPolicy(rest_context, proto_request);
  ASSERT_THAT(backup_success, IsOk());
  EXPECT_THAT(backup_success->version(), Eq(3));
}

TEST(GoldenThingAdminRestStubTest, GetIamPolicy) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  std::string json_response_database(kJsonIamPolicyResponsePayload);
  std::string json_response_backup(kJsonIamPolicyResponsePayload);
  RestContext rest_context;
  google::iam::v1::GetIamPolicyRequest proto_request;
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response_database =
      CreateMockRestResponse(json_response_database);
  auto mock_200_response_backup = CreateMockRestResponse(json_response_backup);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/"
                       "databases/my_database:getIamPolicy"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_database.release());
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/backups/"
                       "my_backup:getIamPolicy"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_backup.release());
      });

  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto database_success = stub.GetIamPolicy(rest_context, proto_request);
  ASSERT_THAT(database_success, IsOk());
  EXPECT_THAT(database_success->bindings(1).role(),
              Eq("roles/resourcemanager.organizationViewer"));
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto backup_success = stub.GetIamPolicy(rest_context, proto_request);
  ASSERT_THAT(backup_success, IsOk());
  EXPECT_THAT(database_success->bindings(1).condition().title(),
              Eq("expirable access"));
}

TEST(GoldenThingAdminRestStubTest, TestIamPermissions) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload = R"({"permissions":["p1","p2","p3"]})";
  std::string json_response_database(kJsonResponsePayload);
  std::string json_response_backup(kJsonResponsePayload);
  RestContext rest_context;
  google::iam::v1::TestIamPermissionsRequest proto_request;
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");

  auto mock_200_response_database =
      CreateMockRestResponse(json_response_database);
  auto mock_200_response_backup = CreateMockRestResponse(json_response_backup);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/"
                       "databases/my_database:testIamPermissions"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_database.release());
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/instances/my_instance/backups/"
                       "my_backup:testIamPermissions"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response_backup.release());
      });

  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto database_success = stub.TestIamPermissions(rest_context, proto_request);
  ASSERT_THAT(database_success, IsOk());
  EXPECT_THAT(database_success->permissions(), ElementsAre("p1", "p2", "p3"));
  proto_request.set_resource(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto backup_success = stub.TestIamPermissions(rest_context, proto_request);
  ASSERT_THAT(backup_success, IsOk());
  EXPECT_THAT(backup_success->permissions(), ElementsAre("p1", "p2", "p3"));
}

TEST(GoldenThingAdminRestStubTest, CreateBackup) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"my_operation","done":"true"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::CreateBackupRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(
            request.path(),
            Eq("/v1/projects/my_project/instances/my_instance/backups"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.CreateBackup(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->name(), Eq("my_operation"));
  EXPECT_TRUE(success->done());
}

TEST(GoldenThingAdminRestStubTest, GetBackup) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"projects/my_project/instances/my_instance/backups/my_backup","state":2})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::GetBackupRequest proto_request;
  proto_request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                   "my_instance/backups/my_backup"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.GetBackup(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(
      success->name(),
      Eq("projects/my_project/instances/my_instance/backups/my_backup"));
  EXPECT_THAT(success->state(),
              Eq(google::test::admin::database::v1::Backup_State_READY));
}

TEST(GoldenThingAdminRestStubTest, UpdateBackup) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"projects/my_project/instances/my_instance/backups/my_backup","state":2})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::UpdateBackupRequest proto_request;
  proto_request.mutable_backup()->set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Patch)
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                       "my_instance/backups/my_backup"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.UpdateBackup(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(
      success->name(),
      Eq("projects/my_project/instances/my_instance/backups/my_backup"));
  EXPECT_THAT(success->state(),
              Eq(google::test::admin::database::v1::Backup_State_READY));
}

TEST(GoldenThingAdminRestStubTest, DeleteBackup) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload = R"({})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::DeleteBackupRequest proto_request;
  proto_request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Delete)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                       "my_instance/backups/my_backup"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.DeleteBackup(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

TEST(GoldenThingAdminRestStubTest, ListBackups) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload = R"(
    {
      "backups":[{"name":"Tom"},{"name":"Dick"},{"name":"Harry"}],
      "next_page_token":"my_next_page_token"
    })";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;

  google::test::admin::database::v1::ListBackupsRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");
  proto_request.set_page_size(100);
  proto_request.set_page_token("my_page_token");
  proto_request.set_filter(
      "(name:howl) AND (create_time < \"2018-03-28T14:50:00Z\")");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(),
                Eq("/v1/projects/my_project/instances/my_instance/backups"));
    EXPECT_THAT(request.GetQueryParameter("page_size"), Contains("100"));
    EXPECT_THAT(request.GetQueryParameter("page_token"),
                Contains("my_page_token"));
    EXPECT_THAT(
        request.GetQueryParameter("filter"),
        Contains("(name:howl) AND (create_time < \"2018-03-28T14:50:00Z\")"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.ListBackups(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  std::vector<std::string> backup_names;
  for (auto const& d : success->backups()) {
    backup_names.push_back(d.name());
  }
  EXPECT_THAT(backup_names, ElementsAre("Tom", "Dick", "Harry"));
  EXPECT_THAT(success->next_page_token(), Eq("my_next_page_token"));
}

TEST(GoldenThingAdminRestStubTest, RestoreDatabase) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload =
      R"({"name":"my_operation","done":"true"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::RestoreDatabaseRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                       "my_instance/databases:restore"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.RestoreDatabase(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->name(), Eq("my_operation"));
  EXPECT_TRUE(success->done());
}

TEST(GoldenThingAdminRestStubTest, ListDatabaseOperations) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload = R"(
    {
      "operations":[{"name":"op1"},{"name":"op2"},{"name":"op3"}],
      "next_page_token":"my_next_page_token"
    })";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;

  google::test::admin::database::v1::ListDatabaseOperationsRequest
      proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");
  proto_request.set_page_size(100);
  proto_request.set_page_token("my_page_token");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                   "my_instance/databaseOperations"));
    EXPECT_THAT(request.GetQueryParameter("page_size"), Contains("100"));
    EXPECT_THAT(request.GetQueryParameter("page_token"),
                Contains("my_page_token"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.ListDatabaseOperations(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  std::vector<std::string> op_names;
  for (auto const& o : success->operations()) {
    op_names.push_back(o.name());
  }
  EXPECT_THAT(op_names, ElementsAre("op1", "op2", "op3"));
  EXPECT_THAT(success->next_page_token(), Eq("my_next_page_token"));
}

TEST(GoldenThingAdminRestStubTest, ListBackupOperations) {
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  auto constexpr kJsonResponsePayload = R"(
    {
      "operations":[{"name":"op1"},{"name":"op2"},{"name":"op3"}],
      "next_page_token":"my_next_page_token"
    })";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;

  google::test::admin::database::v1::ListBackupOperationsRequest proto_request;
  proto_request.set_parent("projects/my_project/instances/my_instance");
  proto_request.set_page_size(100);
  proto_request.set_page_token("my_page_token");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client, Get).WillOnce([&](RestRequest const& request) {
    EXPECT_THAT(request.path(), Eq("/v1/projects/my_project/instances/"
                                   "my_instance/backupOperations"));
    EXPECT_THAT(request.GetQueryParameter("page_size"), Contains("100"));
    EXPECT_THAT(request.GetQueryParameter("page_token"),
                Contains("my_page_token"));
    return std::unique_ptr<rest_internal::RestResponse>(
        mock_200_response.release());
  });
  DefaultGoldenThingAdminRestStub stub(std::move(mock_rest_client), {});
  auto success = stub.ListBackupOperations(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  std::vector<std::string> op_names;
  for (auto const& o : success->operations()) {
    op_names.push_back(o.name());
  }
  EXPECT_THAT(op_names, ElementsAre("op1", "op2", "op3"));
  EXPECT_THAT(success->next_page_token(), Eq("my_next_page_token"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
