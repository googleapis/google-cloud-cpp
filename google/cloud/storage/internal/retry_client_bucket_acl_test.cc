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

TEST(RetryClient, ListBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBucketAcl(ListBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListBucketAcl"));
}

TEST(RetryClient, ListBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListBucketAcl(ListBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListBucketAcl"));
}

TEST(RetryClient, CreateBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucketAcl(CreateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateBucketAcl"));
}

TEST(RetryClient, CreateBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->CreateBucketAcl(CreateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateBucketAcl"));
}

TEST(RetryClient, DeleteBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucketAcl(DeleteBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteBucketAcl"));
}

TEST(RetryClient, DeleteBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteBucketAcl(DeleteBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteBucketAcl"));
}

TEST(RetryClient, GetBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetBucketAcl(GetBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetBucketAcl"));
}

TEST(RetryClient, GetBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->GetBucketAcl(GetBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetBucketAcl"));
}

TEST(RetryClient, UpdateBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucketAcl(UpdateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateBucketAcl"));
}

TEST(RetryClient, UpdateBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateBucketAcl(UpdateBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateBucketAcl"));
}

TEST(RetryClient, PatchBucketAclTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucketAcl)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucketAcl(PatchBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchBucketAcl"));
}

TEST(RetryClient, PatchBucketAclPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchBucketAcl).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchBucketAcl(PatchBucketAclRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchBucketAcl"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
