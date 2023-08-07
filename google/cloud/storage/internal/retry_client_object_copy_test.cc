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

#include "google/cloud/storage/internal/object_requests.h"
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

TEST(RetryClient, CopyObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CopyObject)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CopyObject(CopyObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CopyObject"));
}

TEST(RetryClient, CopyObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CopyObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CopyObject(CopyObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CopyObject"));
}

TEST(RetryClient, ComposeObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ComposeObject)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ComposeObject(ComposeObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ComposeObject"));
}

TEST(RetryClient, ComposeObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ComposeObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ComposeObject(ComposeObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ComposeObject"));
}

TEST(RetryClient, RewriteObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, RewriteObject)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->RewriteObject(RewriteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("RewriteObject"));
}

TEST(RetryClient, RewriteObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, RewriteObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->RewriteObject(RewriteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("RewriteObject"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
