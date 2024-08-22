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
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include <gmock/gmock.h>
#include <memory>
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

TEST(StorageConnectionImpl, ListObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjectAcl(ListObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, ListObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjectAcl(ListObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateObjectAcl(CreateObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateObjectAcl(CreateObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObjectAcl(DeleteObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObjectAcl(DeleteObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetObjectAcl(GetObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetObjectAcl(GetObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObjectAcl(UpdateObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObjectAcl(UpdateObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchObjectAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObjectAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObjectAcl(PatchObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchObjectAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchObjectAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObjectAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObjectAcl(PatchObjectAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchObjectAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
