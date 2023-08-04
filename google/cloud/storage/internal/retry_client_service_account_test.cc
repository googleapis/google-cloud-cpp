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

#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/internal/service_account_requests.h"
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

TEST(RetryClient, GetServiceAccountTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetServiceAccount).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetServiceAccount(GetProjectServiceAccountRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetServiceAccount"));
}

TEST(RetryClient, GetServiceAccountPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetServiceAccount).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetServiceAccount(GetProjectServiceAccountRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetServiceAccount"));
}

TEST(RetryClient, ListHmacKeysTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListHmacKeys).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListHmacKeys(ListHmacKeysRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListHmacKeys"));
}

TEST(RetryClient, ListHmacKeysPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListHmacKeys).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListHmacKeys(ListHmacKeysRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListHmacKeys"));
}

TEST(RetryClient, CreateHmacKeyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateHmacKey).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateHmacKey(CreateHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateHmacKey"));
}

TEST(RetryClient, CreateHmacKeyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateHmacKey).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateHmacKey(CreateHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateHmacKey"));
}

TEST(RetryClient, DeleteHmacKeyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteHmacKey).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteHmacKey(DeleteHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteHmacKey"));
}

TEST(RetryClient, DeleteHmacKeyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteHmacKey).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteHmacKey(DeleteHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteHmacKey"));
}

TEST(RetryClient, GetHmacKeyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetHmacKey).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetHmacKey(GetHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetHmacKey"));
}

TEST(RetryClient, GetHmacKeyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetHmacKey).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetHmacKey(GetHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetHmacKey"));
}

TEST(RetryClient, UpdateHmacKeyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateHmacKey).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateHmacKey(UpdateHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateHmacKey"));
}

TEST(RetryClient, UpdateHmacKeyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateHmacKey).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateHmacKey(UpdateHmacKeyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateHmacKey"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
