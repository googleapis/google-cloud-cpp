// Copyright 2022 Google LLC
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
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
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
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               google::bigtable::v2::ReadRowsRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::SampleRowKeysResponse>>,
              SampleRowKeys,
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               google::bigtable::v2::SampleRowKeysRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::MutateRowResponse>, MutateRow,
              (grpc::ClientContext&, Options const&,
               google::bigtable::v2::MutateRowRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::MutateRowsResponse>>,
              MutateRows,
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               google::bigtable::v2::MutateRowsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>,
              CheckAndMutateRow,
              (grpc::ClientContext&, Options const&,
               google::bigtable::v2::CheckAndMutateRowRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::PingAndWarmResponse>, PingAndWarm,
              (grpc::ClientContext&, Options const&,
               google::bigtable::v2::PingAndWarmRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>,
              ReadModifyWriteRow,
              (grpc::ClientContext&, Options const&,
               google::bigtable::v2::ReadModifyWriteRowRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::bigtable::v2::PrepareQueryResponse>,
              PrepareQuery,
              (grpc::ClientContext&, Options const&,
               google::bigtable::v2::PrepareQueryRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::bigtable::v2::ExecuteQueryResponse>>,
              ExecuteQuery,
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               google::bigtable::v2::ExecuteQueryRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                  google::bigtable::v2::ReadRowsResponse>>,
              AsyncReadRows,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::bigtable::v2::ReadRowsRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                  google::bigtable::v2::SampleRowKeysResponse>>,
              AsyncSampleRowKeys,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::bigtable::v2::SampleRowKeysRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::bigtable::v2::MutateRowResponse>>,
              AsyncMutateRow,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::bigtable::v2::MutateRowRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                  google::bigtable::v2::MutateRowsResponse>>,
              AsyncMutateRows,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::bigtable::v2::MutateRowsRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>,
              AsyncCheckAndMutateRow,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::bigtable::v2::CheckAndMutateRowRequest const&),
              (override));
  MOCK_METHOD(
      future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>,
      AsyncReadModifyWriteRow,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       google::bigtable::v2::ReadModifyWriteRowRequest const&),
      (override));
};

class MockMutateRowsStream : public google::cloud::internal::StreamingReadRpc<
                                 google::bigtable::v2::MutateRowsResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(absl::optional<Status>, Read,
              (google::bigtable::v2::MutateRowsResponse*), (override));
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
};

class MockReadRowsStream : public google::cloud::internal::StreamingReadRpc<
                               google::bigtable::v2::ReadRowsResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(absl::optional<Status>, Read,
              (google::bigtable::v2::ReadRowsResponse*), (override));
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
};

class MockSampleRowKeysStream
    : public google::cloud::internal::StreamingReadRpc<
          google::bigtable::v2::SampleRowKeysResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(absl::optional<Status>, Read,
              (google::bigtable::v2::SampleRowKeysResponse*), (override));
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
};

using MockAsyncMutateRowsStream =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<
        google::bigtable::v2::MutateRowsResponse>;
using MockAsyncReadRowsStream =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<
        google::bigtable::v2::ReadRowsResponse>;
using MockAsyncSampleRowKeysStream =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<
        google::bigtable::v2::SampleRowKeysResponse>;

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BIGTABLE_STUB_H
