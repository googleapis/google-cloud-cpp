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

#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/connection_impl.h"
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

TEST(StorageConnectionImpl, ListBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBucketAcl(ListBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, ListBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBucketAcl(ListBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucketAcl(CreateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucketAcl(CreateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucketAcl(DeleteBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucketAcl(DeleteBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetBucketAcl(GetBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetBucketAcl(GetBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucketAcl(UpdateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucketAcl(UpdateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchBucketAclTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucketAcl).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucketAcl(PatchBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchBucketAcl"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchBucketAclPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucketAcl).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucketAcl(PatchBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchBucketAcl"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
