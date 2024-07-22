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

#include "google/cloud/storage/internal/bucket_requests.h"
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

TEST(StorageConnectionImpl, ListBucketsTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBuckets).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBuckets(ListBucketsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListBuckets"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, ListBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBuckets).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBuckets(ListBucketsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListBuckets"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateBucketTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucket).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucket(CreateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateBucket"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, CreateBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucket).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucket(CreateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateBucket"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteBucketTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucket).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucket(DeleteBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteBucket"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, DeleteBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucket).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucket(DeleteBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteBucket"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetBucketTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketMetadata).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetBucketMetadata(GetBucketMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetBucketMetadata"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketMetadata).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetBucketMetadata(GetBucketMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetBucketMetadata"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateBucketTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucket).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucket(UpdateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateBucket"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, UpdateBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucket).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucket(UpdateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateBucket"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchBucketTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucket).Times(3).WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucket(PatchBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchBucket"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, PatchBucketPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucket).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucket(PatchBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchBucket"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetNativeBucketIamPolicyTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNativeBucketIamPolicy)
      .Times(3)
      .WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetNativeBucketIamPolicy"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, GetNativeBucketIamPolicyPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNativeBucketIamPolicy).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetNativeBucketIamPolicy"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, SetNativeBucketIamPolicyTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SetNativeBucketIamPolicy)
      .Times(3)
      .WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("SetNativeBucketIamPolicy"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, SetNativeBucketIamPolicyPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SetNativeBucketIamPolicy).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("SetNativeBucketIamPolicy"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, TestBucketIamPermissionsTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, TestBucketIamPermissions)
      .Times(3)
      .WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->TestBucketIamPermissions(TestBucketIamPermissionsRequest())
          .status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("TestBucketIamPermissions"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, TestBucketIamPermissionsPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, TestBucketIamPermissions).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->TestBucketIamPermissions(TestBucketIamPermissionsRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("TestBucketIamPermissions"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, LockBucketRetentionPolicyTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, LockBucketRetentionPolicy)
      .Times(3)
      .WillRepeatedly(transient);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->LockBucketRetentionPolicy(LockBucketRetentionPolicyRequest())
          .status();
  EXPECT_THAT(response,
              StoppedOnTooManyTransients("LockBucketRetentionPolicy"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(transient.captured_authority_options(), RetryLoopUsesOptions());
}

TEST(StorageConnectionImpl, LockBucketRetentionPolicyPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, LockBucketRetentionPolicy).WillOnce(permanent);
  auto client =
      StorageConnectionImpl::Create(std::move(mock), RetryTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->LockBucketRetentionPolicy(LockBucketRetentionPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("LockBucketRetentionPolicy"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
  EXPECT_THAT(permanent.captured_authority_options(), RetryLoopUsesOptions());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
