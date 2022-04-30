// Copyright 2022 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BIGTABLE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BIGTABLE_STUB_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockBigtableStub : public bigtable_internal::BigtableStub {
 public:
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::ReadRowsResponse>>,
              ReadRows,
              (std::unique_ptr<grpc::ClientContext>,
               google::bigtable::v2::ReadRowsRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::SampleRowKeysResponse>>,
              SampleRowKeys,
              (std::unique_ptr<grpc::ClientContext>,
               google::bigtable::v2::SampleRowKeysRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::MutateRowResponse>, MutateRow,
              (grpc::ClientContext&,
               google::bigtable::v2::MutateRowRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::MutateRowsResponse>>,
              MutateRows,
              (std::unique_ptr<grpc::ClientContext>,
               google::bigtable::v2::MutateRowsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>,
              CheckAndMutateRow,
              (grpc::ClientContext&,
               google::bigtable::v2::CheckAndMutateRowRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::PingAndWarmResponse>, PingAndWarm,
              (grpc::ClientContext&,
               google::bigtable::v2::PingAndWarmRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>,
              ReadModifyWriteRow,
              (grpc::ClientContext&,
               google::bigtable::v2::ReadModifyWriteRowRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::bigtable::v2::MutateRowResponse>>,
              AsyncMutateRow,
              (google::cloud::CompletionQueue&,
               std::unique_ptr<grpc::ClientContext>,
               google::bigtable::v2::MutateRowRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>,
              AsyncCheckAndMutateRow,
              (google::cloud::CompletionQueue&,
               std::unique_ptr<grpc::ClientContext>,
               google::bigtable::v2::CheckAndMutateRowRequest const&),
              (override));
  MOCK_METHOD(
      future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>,
      AsyncReadModifyWriteRow,
      (google::cloud::CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
       google::bigtable::v2::ReadModifyWriteRowRequest const&),
      (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BIGTABLE_STUB_H
