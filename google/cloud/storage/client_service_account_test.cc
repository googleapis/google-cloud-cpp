// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

Status PermanentError() {
  // We need an error code different from `kInvalidArgument` as this is used
  // by `storage_internal::RequestProjectId()`. Some of the tests could be
  // testing the wrong thing if we used the same value.
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

/**
 * Test the Projects.serviceAccount-related functions in storage::Client.
 */
class ServiceAccountTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(ServiceAccountTest, GetProjectServiceAccount) {
  ServiceAccount expected =
      internal::ServiceAccountParser::FromString(
          R"""({"email_address": "test-service-account@test-domain.com"})""")
          .value();

  EXPECT_CALL(*mock_, GetServiceAccount)
      .WillOnce(Return(StatusOr<ServiceAccount>(TransientError())))
      .WillOnce(
          [&expected](internal::GetProjectServiceAccountRequest const& r) {
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
            EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
            EXPECT_EQ("test-project", r.project_id());

            return make_status_or(expected);
          });
  auto client = ClientForMock();
  StatusOr<ServiceAccount> actual = client.GetServiceAccountForProject(
      "test-project", Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, GetServiceAccount).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetServiceAccount(Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ServiceAccountTest,
       GetProjectServiceAccountProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetProjectServiceAccountRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetServiceAccount(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.GetServiceAccount(Options{}.set<UserProjectOption>("u-p-test"));
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetProjectServiceAccountRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetServiceAccount(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetServiceAccount(Options{});
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetProjectServiceAccountRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetServiceAccount(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetServiceAccount(
      Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetProjectServiceAccountRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetServiceAccount(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetServiceAccount(
      OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, ListHmacKeysNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, ListHmacKeys).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListHmacKeys(Options{});
  std::vector<StatusOr<HmacKeyMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(StatusIs(StatusCode::kInvalidArgument)));
}

TEST_F(ServiceAccountTest, ListHmacKeysProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListHmacKeysRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListHmacKeys(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListHmacKeys(Options{});
  std::vector<StatusOr<HmacKeyMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(ServiceAccountTest, ListHmacKeysProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListHmacKeysRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListHmacKeys(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.ListHmacKeys(Options{});
  std::vector<StatusOr<HmacKeyMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(ServiceAccountTest, ListHmacKeysProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListHmacKeysRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListHmacKeys(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.ListHmacKeys(Options{}.set<ProjectIdOption>("call-project-id"));
  std::vector<StatusOr<HmacKeyMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(ServiceAccountTest, ListHmacKeysProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::ListHmacKeysRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, ListHmacKeys(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.ListHmacKeys(OverrideDefaultProject("override-project-id"),
                          Options{}.set<ProjectIdOption>("call-project-id"));
  std::vector<StatusOr<HmacKeyMetadata>> list{actual.begin(), actual.end()};
  EXPECT_THAT(list, ElementsAre(PermanentError()));
}

TEST_F(ServiceAccountTest, CreateHmacKeyNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, CreateHmacKey).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.CreateHmacKey("test-service-account", Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ServiceAccountTest, CreateHmacKeyProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateHmacKeyRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.CreateHmacKey("test-service-account", Options{});
}

TEST_F(ServiceAccountTest, CreateHmacKeyProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateHmacKeyRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.CreateHmacKey("test-service-account", Options{});
}

TEST_F(ServiceAccountTest, CreateHmacKeyProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateHmacKeyRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.CreateHmacKey("test-service-account",
                           Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, CreateHmacKeyProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::CreateHmacKeyRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, CreateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.CreateHmacKey(
      "test-service-account", OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, CreateHmacKey) {
  internal::CreateHmacKeyResponse expected =
      internal::CreateHmacKeyResponse::FromHttpResponse(
          R"""({"secretKey": "dGVzdC1zZWNyZXQ=", "resource": {}})""")
          .value();

  EXPECT_CALL(*mock_, CreateHmacKey)
      .WillOnce(
          Return(StatusOr<internal::CreateHmacKeyResponse>(TransientError())))
      .WillOnce([&expected](internal::CreateHmacKeyRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-service-account", r.service_account());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<std::pair<HmacKeyMetadata, std::string>> actual =
      client.CreateHmacKey("test-service-account",
                           OverrideDefaultProject("test-project"),
                           Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.metadata, actual->first);
  EXPECT_EQ(expected.secret, actual->second);
}

TEST_F(ServiceAccountTest, DeleteHmacKeyNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, DeleteHmacKey).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.DeleteHmacKey("test-access-id", Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ServiceAccountTest, DeleteHmacKeyProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::DeleteHmacKeyRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, DeleteHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.DeleteHmacKey("test-access-id", Options{});
}

TEST_F(ServiceAccountTest, DeleteHmacKeyProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::DeleteHmacKeyRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, DeleteHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.DeleteHmacKey("test-access-id", Options{});
}

TEST_F(ServiceAccountTest, DeleteHmacKeyProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::DeleteHmacKeyRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, DeleteHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.DeleteHmacKey(
      "test-access-id", Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, DeleteHmacKeyProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::DeleteHmacKeyRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, DeleteHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.DeleteHmacKey(
      "test-access-id", OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, DeleteHmacKey) {
  EXPECT_CALL(*mock_, DeleteHmacKey)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteHmacKeyRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  Status actual = client.DeleteHmacKey(
      "test-access-id-1", OverrideDefaultProject("test-project"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
}

TEST_F(ServiceAccountTest, GetHmacKeyNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, GetHmacKey).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetHmacKey("test-access-id", Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ServiceAccountTest, GetHmacKeyProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetHmacKeyRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetHmacKey("test-access-id", Options{});
}

TEST_F(ServiceAccountTest, GetHmacKeyProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetHmacKeyRequest::project_id, "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetHmacKey("test-access-id", Options{});
}

TEST_F(ServiceAccountTest, GetHmacKeyProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetHmacKeyRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetHmacKey(
      "test-access-id", Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, GetHmacKeyProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::GetHmacKeyRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, GetHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.GetHmacKey(
      "test-access-id", OverrideDefaultProject("override-project-id"),
      Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, GetHmacKey) {
  HmacKeyMetadata expected =
      internal::HmacKeyMetadataParser::FromString(
          R"""({"accessId": "test-access-id-1", "state": "ACTIVE"})""")
          .value();

  EXPECT_CALL(*mock_, GetHmacKey)
      .WillOnce(Return(StatusOr<HmacKeyMetadata>(TransientError())))
      .WillOnce([&expected](internal::GetHmacKeyRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<HmacKeyMetadata> actual = client.GetHmacKey(
      "test-access-id-1", OverrideDefaultProject("test-project"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.access_id(), actual->access_id());
  EXPECT_EQ(expected.state(), actual->state());
}

TEST_F(ServiceAccountTest, UpdateHmacKeyNoProject) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(
          Return(storage::internal::DefaultOptionsWithCredentials(Options{})));
  EXPECT_CALL(*mock, UpdateHmacKey).Times(0);
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.UpdateHmacKey("test-service-account", HmacKeyMetadata(),
                                     Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ServiceAccountTest, UpdateHmacKeyProjectFromConnectionOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::UpdateHmacKeyRequest::project_id,
                    "client-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, UpdateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.UpdateHmacKey("test-service-account", HmacKeyMetadata(),
                                     Options{});
}

TEST_F(ServiceAccountTest, UpdateHmacKeyProjectFromEnv) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::UpdateHmacKeyRequest::project_id,
                    "env-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, UpdateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual = client.UpdateHmacKey("test-service-account", HmacKeyMetadata(),
                                     Options{});
}

TEST_F(ServiceAccountTest, UpdateHmacKeyProjectFromCallOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::UpdateHmacKeyRequest::project_id,
                    "call-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, UpdateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.UpdateHmacKey("test-service-account", HmacKeyMetadata(),
                           Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, UpdateHmacKeyProjectFromOverride) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", "env-project-id");
  auto mock = std::make_shared<testing::MockClient>();
  auto expected_request = []() {
    return Property(&internal::UpdateHmacKeyRequest::project_id,
                    "override-project-id");
  };
  EXPECT_CALL(*mock, options())
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}.set<ProjectIdOption>("client-project-id"))));
  EXPECT_CALL(*mock, UpdateHmacKey(expected_request()))
      .WillOnce(Return(PermanentError()));
  auto client =
      google::cloud::storage::testing::UndecoratedClientFromMock(mock);
  auto actual =
      client.UpdateHmacKey("test-service-account", HmacKeyMetadata(),
                           OverrideDefaultProject("override-project-id"),
                           Options{}.set<ProjectIdOption>("call-project-id"));
}

TEST_F(ServiceAccountTest, UpdateHmacKey) {
  HmacKeyMetadata expected =
      internal::HmacKeyMetadataParser::FromString(
          R"""({"accessId": "test-access-id-1", "state": "ACTIVE"})""")
          .value();

  EXPECT_CALL(*mock_, UpdateHmacKey)
      .WillOnce(Return(StatusOr<HmacKeyMetadata>(TransientError())))
      .WillOnce([&expected](internal::UpdateHmacKeyRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<HmacKeyMetadata> actual = client.UpdateHmacKey(
      "test-access-id-1", HmacKeyMetadata().set_state("ACTIVE"),
      OverrideDefaultProject("test-project"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.access_id(), actual->access_id());
  EXPECT_EQ(expected.state(), actual->state());
}

TEST(DefaultCtorsWork, Trivial) {
  EXPECT_FALSE(OverrideDefaultProject().has_value());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
