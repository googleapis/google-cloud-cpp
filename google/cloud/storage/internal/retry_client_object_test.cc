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
using ::google::cloud::storage::testing::MockRetryClientFunction;
using ::google::cloud::storage::testing::RetryClientTestOptions;
using ::google::cloud::storage::testing::RetryLoopUsesSingleToken;
using ::google::cloud::storage::testing::StoppedOnPermanentError;
using ::google::cloud::storage::testing::StoppedOnTooManyTransients;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;

TEST(RetryClient, InsertObjectMediaTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertObjectMedia).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->InsertObjectMedia(InsertObjectMediaRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("InsertObjectMedia"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, InsertObjectMediaPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertObjectMedia).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->InsertObjectMedia(InsertObjectMediaRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("InsertObjectMedia"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, GetObjectMetadataTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectMetadata).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetObjectMetadata(GetObjectMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetObjectMetadata"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, GetObjectMetadataPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectMetadata).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetObjectMetadata(GetObjectMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetObjectMetadata"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, ListObjectsTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjects).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjects(ListObjectsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListObjects"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, ListObjectsPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjects).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjects(ListObjectsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListObjects"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, ReadObjectTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadObject).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ReadObject(ReadObjectRangeRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ReadObjectNotWrapped"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, ReadObjectPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadObject).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ReadObject(ReadObjectRangeRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ReadObjectNotWrapped"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, CreateResumableUploadTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateResumableUpload).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateResumableUpload(ResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateResumableUpload"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, CreateResumableUploadPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateResumableUpload).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateResumableUpload(ResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateResumableUpload"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, QueryResumableUploadTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResumableUpload).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->QueryResumableUpload(QueryResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("QueryResumableUpload"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, QueryResumableUploadPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResumableUpload).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->QueryResumableUpload(QueryResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("QueryResumableUpload"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, DeleteResumableUploadTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteResumableUpload).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteResumableUpload(DeleteResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteResumableUpload"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, DeleteResumableUploadPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteResumableUpload).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteResumableUpload(DeleteResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteResumableUpload"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, DeleteObjectTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObject).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObject(DeleteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteObject"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, DeleteObjectPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObject).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObject(DeleteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteObject"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, UpdateObjectTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObject).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObject(UpdateObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateObject"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, UpdateObjectPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObject).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObject(UpdateObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateObject"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, PatchObjectTooManyFailures) {
  auto transient = MockRetryClientFunction(TransientError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObject).Times(3).WillRepeatedly(transient);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObject(PatchObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchObject"));
  EXPECT_THAT(transient.captured_tokens(), RetryLoopUsesSingleToken());
}

TEST(RetryClient, PatchObjectPermanentFailure) {
  auto permanent = MockRetryClientFunction(PermanentError());
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObject).WillOnce(permanent);
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObject(PatchObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchObject"));
  EXPECT_THAT(permanent.captured_tokens(), RetryLoopUsesSingleToken());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
