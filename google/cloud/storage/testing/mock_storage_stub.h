// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H

#include "google/cloud/storage/internal/storage_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockStorageStub : public internal::StorageStub {
 public:
  MOCK_METHOD(std::unique_ptr<ReadObjectStream>, ReadObject,
              (std::unique_ptr<grpc::ClientContext>,
               google::storage::v2::ReadObjectRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<WriteObjectStream>, WriteObject,
              (std::unique_ptr<grpc::ClientContext>), (override));
  MOCK_METHOD(StatusOr<google::storage::v2::StartResumableWriteResponse>,
              StartResumableWrite,
              (grpc::ClientContext&,
               google::storage::v2::StartResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::QueryWriteStatusResponse>,
              QueryWriteStatus,
              (grpc::ClientContext&,
               google::storage::v2::QueryWriteStatusRequest const&),
              (override));
};

class MockInsertStream : public internal::StorageStub::WriteObjectStream {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write,
              (google::storage::v2::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::WriteObjectResponse>, Close, (),
              (override));
};

class MockObjectMediaStream : public internal::StorageStub::ReadObjectStream {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  using ReadResultType =
      absl::variant<Status, google::storage::v2::ReadObjectResponse>;
  MOCK_METHOD(ReadResultType, Read, (), (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
