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

TEST(RetryClient, ListBucketsTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBuckets)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBuckets(ListBucketsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListBuckets"));
}

TEST(RetryClient, ListBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBuckets).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBuckets(ListBucketsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListBuckets"));
}

TEST(RetryClient, CreateBucketTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucket)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucket(CreateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateBucket"));
}

TEST(RetryClient, CreateBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucket).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucket(CreateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateBucket"));
}

TEST(RetryClient, DeleteBucketTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucket)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucket(DeleteBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteBucket"));
}

TEST(RetryClient, DeleteBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucket).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucket(DeleteBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteBucket"));
}

TEST(RetryClient, GetBucketTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketMetadata)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetBucketMetadata(GetBucketMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetBucketMetadata"));
}

TEST(RetryClient, GetBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketMetadata).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetBucketMetadata(GetBucketMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetBucketMetadata"));
}

TEST(RetryClient, UpdateBucketTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucket)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucket(UpdateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateBucket"));
}

TEST(RetryClient, UpdateBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucket).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucket(UpdateBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateBucket"));
}

TEST(RetryClient, PatchBucketTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucket)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucket(PatchBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchBucket"));
}

TEST(RetryClient, PatchBucketPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucket).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucket(PatchBucketRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchBucket"));
}

TEST(RetryClient, GetNativeBucketIamPolicyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNativeBucketIamPolicy)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetNativeBucketIamPolicy"));
}

TEST(RetryClient, GetNativeBucketIamPolicyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetNativeBucketIamPolicy)
      .WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetNativeBucketIamPolicy"));
}

TEST(RetryClient, SetNativeBucketIamPolicyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SetNativeBucketIamPolicy)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("SetNativeBucketIamPolicy"));
}

TEST(RetryClient, SetNativeBucketIamPolicyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SetNativeBucketIamPolicy)
      .WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("SetNativeBucketIamPolicy"));
}

TEST(RetryClient, TestBucketIamPermissionsTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, TestBucketIamPermissions)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->TestBucketIamPermissions(TestBucketIamPermissionsRequest())
          .status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("TestBucketIamPermissions"));
}

TEST(RetryClient, TestBucketIamPermissionsPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, TestBucketIamPermissions)
      .WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->TestBucketIamPermissions(TestBucketIamPermissionsRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("TestBucketIamPermissions"));
}

TEST(RetryClient, LockBucketRetentionPolicyTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, LockBucketRetentionPolicy)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->LockBucketRetentionPolicy(LockBucketRetentionPolicyRequest())
          .status();
  EXPECT_THAT(response,
              StoppedOnTooManyTransients("LockBucketRetentionPolicy"));
}

TEST(RetryClient, LockBucketRetentionPolicyPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, LockBucketRetentionPolicy)
      .WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->LockBucketRetentionPolicy(LockBucketRetentionPolicyRequest())
          .status();
  EXPECT_THAT(response, StoppedOnPermanentError("LockBucketRetentionPolicy"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
