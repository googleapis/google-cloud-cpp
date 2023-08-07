// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::RetryClientTestOptions;
using ::google::cloud::storage::testing::StoppedOnPermanentError;
using ::google::cloud::storage::testing::StoppedOnTooManyTransients;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::Return;

TEST(RetryClient, ListNotificationTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListNotifications)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListNotifications(ListNotificationsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListNotifications"));
}

TEST(RetryClient, ListNotificationPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListNotifications).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListNotifications(ListNotificationsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListNotifications"));
}

TEST(RetryClient, CreateNotificationTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateNotification)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateNotification(CreateNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateNotification"));
}

TEST(RetryClient, CreateNotificationPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateNotification).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateNotification(CreateNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateNotification"));
}

TEST(RetryClient, DeleteNotificationTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteNotification)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteNotification(DeleteNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteNotification"));
}

TEST(RetryClient, DeleteNotificationPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteNotification).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteNotification(DeleteNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteNotification"));
}

TEST(RetryClient, GetNotificationTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNotification)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetNotification(GetNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetNotification"));
}

TEST(RetryClient, GetNotificationPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNotification).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetNotification(GetNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetNotification"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
