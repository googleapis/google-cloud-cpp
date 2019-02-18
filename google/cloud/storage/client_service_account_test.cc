// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::oauth2::CreateAnonymousCredentials;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;

/**
 * Test the Projects.serviceAccount-related functions in storage::Client.
 */
class ServiceAccountTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options = ClientOptions(CreateAnonymousCredentials());
};

TEST_F(ServiceAccountTest, GetProjectServiceAccount) {
  ServiceAccount expected =
      ServiceAccount::ParseFromString(
          R"""({"email_address": "test-service-account@test-domain.com"})""")
          .value();

  EXPECT_CALL(*mock, GetServiceAccount(_))
      .WillOnce(Return(StatusOr<ServiceAccount>(TransientError())))
      .WillOnce(Invoke(
          [&expected](internal::GetProjectServiceAccountRequest const& r) {
            EXPECT_EQ("test-project", r.project_id());

            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<ServiceAccount> actual =
      client.GetServiceAccountForProject("test-project");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountTooManyFailures) {
  testing::TooManyFailuresStatusTest<ServiceAccount>(
      mock, EXPECT_CALL(*mock, GetServiceAccount(_)),
      [](Client& client) {
        return client.GetServiceAccountForProject("test-project").status();
      },
      "GetServiceAccount");
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountPermanentFailure) {
  testing::PermanentFailureStatusTest<ServiceAccount>(
      *client, EXPECT_CALL(*mock, GetServiceAccount(_)),
      [](Client& client) {
        return client.GetServiceAccountForProject("test-project").status();
      },
      "GetServiceAccount");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
