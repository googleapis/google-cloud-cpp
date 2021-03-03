// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_DATA_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_DATA_CLIENT_H

#include "google/cloud/bigtable/table.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockDataClient : public bigtable::DataClient {
 public:
  MOCK_METHOD(std::string const&, project_id, (), (const override));
  MOCK_METHOD(std::string const&, instance_id, (), (const override));
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, Channel, (), (override));
  MOCK_METHOD(void, reset, (), (override));

  MOCK_METHOD(grpc::Status, MutateRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::MutateRowRequest const& request,
               google::bigtable::v2::MutateRowResponse* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::v2::MutateRowResponse>>,
              AsyncMutateRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::MutateRowRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(grpc::Status, CheckAndMutateRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::CheckAndMutateRowRequest const& request,
               google::bigtable::v2::CheckAndMutateRowResponse* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::v2::CheckAndMutateRowResponse>>,
              AsyncCheckAndMutateRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::CheckAndMutateRowRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(grpc::Status, ReadModifyWriteRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::ReadModifyWriteRowRequest const& request,
               google::bigtable::v2::ReadModifyWriteRowResponse* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::v2::ReadModifyWriteRowResponse>>,
              AsyncReadModifyWriteRow,
              (grpc::ClientContext * context,
               google::bigtable::v2::ReadModifyWriteRowRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>,
      ReadRows,
      (grpc::ClientContext * context,
       google::bigtable::v2::ReadRowsRequest const& request),
      (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncReaderInterface<
                  google::bigtable::v2::ReadRowsResponse>>,
              AsyncReadRows,
              (grpc::ClientContext*,
               const google::bigtable::v2::ReadRowsRequest&,
               grpc::CompletionQueue*, void*),
              (override));
  MOCK_METHOD(std::unique_ptr<::grpc::ClientAsyncReaderInterface<
                  ::google::bigtable::v2::ReadRowsResponse>>,
              PrepareAsyncReadRows,
              (::grpc::ClientContext*,
               const ::google::bigtable::v2::ReadRowsRequest&,
               ::grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientReaderInterface<
                  google::bigtable::v2::SampleRowKeysResponse>>,
              SampleRowKeys,
              (grpc::ClientContext * context,
               google::bigtable::v2::SampleRowKeysRequest const& request),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncReaderInterface<
                  google::bigtable::v2::SampleRowKeysResponse>>,
              AsyncSampleRowKeys,
              (grpc::ClientContext*,
               const google::bigtable::v2::SampleRowKeysRequest&,
               grpc::CompletionQueue*, void*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientReaderInterface<
                  google::bigtable::v2::MutateRowsResponse>>,
              MutateRows,
              (grpc::ClientContext * context,
               google::bigtable::v2::MutateRowsRequest const& request),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncReaderInterface<
                  google::bigtable::v2::MutateRowsResponse>>,
              AsyncMutateRows,
              (grpc::ClientContext*,
               const google::bigtable::v2::MutateRowsRequest&,
               grpc::CompletionQueue*, void*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncReaderInterface<
                  google::bigtable::v2::MutateRowsResponse>>,
              PrepareAsyncMutateRows,
              (grpc::ClientContext*,
               const google::bigtable::v2::MutateRowsRequest&,
               grpc::CompletionQueue*),
              (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_DATA_CLIENT_H
