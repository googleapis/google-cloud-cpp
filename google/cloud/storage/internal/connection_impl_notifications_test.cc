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

#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::MockRetryClientFunction;
using ::google::cloud::storage::testing::RetryLoopUsesOptions;
using ::google::cloud::storage::testing::RetryLoopUsesSingleToken;
using ::google::cloud::storage::testing::RetryTestOptions;
using ::google::cloud::storage::testing::StoppedOnPermanentError;
using ::google::cloud::storage::testing::StoppedOnTooManyTransients;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;

TEST(StorageConnectionImpl, ListNotificationTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListNotifications).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListNotifications(ListNotificationsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListNotifications"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, ListNotificationPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListNotifications).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->ListNotifications(ListNotificationsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListNotifications"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateNotificationTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateNotification).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateNotification(CreateNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateNotification"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateNotificationPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateNotification).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateNotification(CreateNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateNotification"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteNotificationTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteNotification).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteNotification(DeleteNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteNotification"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteNotificationPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteNotification).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteNotification(DeleteNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteNotification"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetNotificationTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNotification).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetNotification(GetNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetNotification"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetNotificationPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNotification).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetNotification(GetNotificationRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetNotification"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
