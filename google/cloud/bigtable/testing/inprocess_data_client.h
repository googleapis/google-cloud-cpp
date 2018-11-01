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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_DATA_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_DATA_CLIENT_H_

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/data_client.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/**
 * Connect to an embedded Cloud Bigtable server implementing the data
 * manipulation APIs.
 *
 * This class is mainly for testing purpose, it enable use of a single embedded
 * server
 * for multiple test cases. This dataclient uses a pre-defined channel.
 */
class InProcessDataClient : public bigtable::DataClient {
 public:
  InProcessDataClient(std::string project, std::string instance,
                      std::shared_ptr<grpc::Channel> channel)
      : project_(std::move(project)),
        instance_(std::move(instance)),
        channel_(std::move(channel)) {}

  using BigtableStubPtr =
      std::shared_ptr<google::bigtable::v2::Bigtable::StubInterface>;

  std::string const& project_id() const override { return project_; }
  std::string const& instance_id() const override { return instance_; }
  std::shared_ptr<grpc::Channel> Channel() override { return channel_; }
  void reset() override {}

  std::unique_ptr<google::bigtable::v2::Bigtable::Stub> Stub() {
    return google::bigtable::v2::Bigtable::NewStub(Channel());
  }

  //@{
  /// @name the google.bigtable.v2.Bigtable operations.
  grpc::Status MutateRow(
      grpc::ClientContext* context,
      google::bigtable::v2::MutateRowRequest const& request,
      google::bigtable::v2::MutateRowResponse* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::MutateRowResponse>>
  AsyncMutateRow(grpc::ClientContext* context,
                 google::bigtable::v2::MutateRowRequest const& request,
                 grpc::CompletionQueue* cq) override;
  grpc::Status CheckAndMutateRow(
      grpc::ClientContext* context,
      google::bigtable::v2::CheckAndMutateRowRequest const& request,
      google::bigtable::v2::CheckAndMutateRowResponse* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::CheckAndMutateRowResponse>>
  AsyncCheckAndMutateRow(
      grpc::ClientContext* context,
      const google::bigtable::v2::CheckAndMutateRowRequest& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status ReadModifyWriteRow(
      grpc::ClientContext* context,
      google::bigtable::v2::ReadModifyWriteRowRequest const& request,
      google::bigtable::v2::ReadModifyWriteRowResponse* response) override;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>
  ReadRows(grpc::ClientContext* context,
           google::bigtable::v2::ReadRowsRequest const& request) override;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::SampleRowKeysResponse>>
  SampleRowKeys(
      grpc::ClientContext* context,
      google::bigtable::v2::SampleRowKeysRequest const& request) override;
  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::SampleRowKeysResponse>>
  AsyncSampleRowKeys(
      ::grpc::ClientContext* context,
      const ::google::bigtable::v2::SampleRowKeysRequest& request,
      ::grpc::CompletionQueue* cq, void* tag) override;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::MutateRowsResponse>>
  MutateRows(grpc::ClientContext* context,
             google::bigtable::v2::MutateRowsRequest const& request) override;
  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::MutateRowsResponse>>
  AsyncMutateRows(::grpc::ClientContext* context,
                  const ::google::bigtable::v2::MutateRowsRequest& request,
                  ::grpc::CompletionQueue* cq, void* tag) override;
  //@}

 private:
  std::string project_;
  std::string instance_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_DATA_CLIENT_H_
