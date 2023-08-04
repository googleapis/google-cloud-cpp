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
using ::testing::ByMove;
using ::testing::Return;

TEST(RetryClient, InsertObjectMediaTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertObjectMedia)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->InsertObjectMedia(InsertObjectMediaRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("InsertObjectMedia"));
}

TEST(RetryClient, InsertObjectMediaPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertObjectMedia).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->InsertObjectMedia(InsertObjectMediaRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("InsertObjectMedia"));
}

TEST(RetryClient, GetObjectMetadataTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectMetadata)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetObjectMetadata(GetObjectMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("GetObjectMetadata"));
}

TEST(RetryClient, GetObjectMetadataPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetObjectMetadata).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->GetObjectMetadata(GetObjectMetadataRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("GetObjectMetadata"));
}

TEST(RetryClient, ListObjectsTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjects)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjects(ListObjectsRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ListObjects"));
}

TEST(RetryClient, ListObjectsPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListObjects).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ListObjects(ListObjectsRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ListObjects"));
}

TEST(RetryClient, ReadObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadObject).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ReadObject(ReadObjectRangeRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("ReadObjectNotWrapped"));
}

TEST(RetryClient, ReadObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadObject).WillOnce(Return(ByMove(PermanentError())));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->ReadObject(ReadObjectRangeRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("ReadObjectNotWrapped"));
}

TEST(RetryClient, CreateResumableUploadTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateResumableUpload).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateResumableUpload(ResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("CreateResumableUpload"));
}

TEST(RetryClient, CreateResumableUploadPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateResumableUpload).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->CreateResumableUpload(ResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("CreateResumableUpload"));
}

TEST(RetryClient, DeleteResumableUploadTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteResumableUpload).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteResumableUpload(DeleteResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteResumableUpload"));
}

TEST(RetryClient, DeleteResumableUploadPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteResumableUpload).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response =
      client->DeleteResumableUpload(DeleteResumableUploadRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteResumableUpload"));
}

TEST(RetryClient, DeleteObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObject).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObject(DeleteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("DeleteObject"));
}

TEST(RetryClient, DeleteObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->DeleteObject(DeleteObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("DeleteObject"));
}

TEST(RetryClient, UpdateObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObject).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObject(UpdateObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("UpdateObject"));
}

TEST(RetryClient, UpdateObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, UpdateObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->UpdateObject(UpdateObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("UpdateObject"));
}

TEST(RetryClient, PatchObjectTooManyFailures) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObject).Times(3).WillRepeatedly([] {
    return TransientError();
  });
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObject(PatchObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnTooManyTransients("PatchObject"));
}

TEST(RetryClient, PatchObjectPermanentFailure) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, PatchObject).WillOnce(Return(PermanentError()));
  auto client = RetryClient::Create(std::move(mock), RetryClientTestOptions());
  google::cloud::internal::OptionsSpan span(client->options());
  auto response = client->PatchObject(PatchObjectRequest()).status();
  EXPECT_THAT(response, StoppedOnPermanentError("PatchObject"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
