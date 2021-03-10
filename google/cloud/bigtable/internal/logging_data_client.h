// Copyright 2020 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_DATA_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_DATA_CLIENT_H

#include "google/cloud/bigtable/data_client.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btproto = google::bigtable::v2;

/**
 * Implement a logging DataClient.
 *
 * This implementation does not support multiple threads, or refresh
 * authorization tokens.  In other words, it is extremely bare bones.
 */
class LoggingDataClient : public DataClient {
 public:
  LoggingDataClient(std::shared_ptr<google::cloud::bigtable::DataClient> child,
                    google::cloud::TracingOptions options)
      : child_(std::move(child)), tracing_options_(std::move(options)) {}

  std::string const& project_id() const override {
    return child_->project_id();
  }

  std::string const& instance_id() const override {
    return child_->instance_id();
  }

  std::shared_ptr<grpc::Channel> Channel() override {
    return child_->Channel();
  }

  void reset() override { child_->reset(); }

  grpc::Status MutateRow(grpc::ClientContext* context,
                         btproto::MutateRowRequest const& request,
                         btproto::MutateRowResponse* response) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>
  AsyncMutateRow(grpc::ClientContext* context,
                 btproto::MutateRowRequest const& request,
                 grpc::CompletionQueue* cq) override;

  grpc::Status CheckAndMutateRow(
      grpc::ClientContext* context,
      btproto::CheckAndMutateRowRequest const& request,
      btproto::CheckAndMutateRowResponse* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::CheckAndMutateRowResponse>>
  AsyncCheckAndMutateRow(
      grpc::ClientContext* context,
      const google::bigtable::v2::CheckAndMutateRowRequest& request,
      grpc::CompletionQueue* cq) override;

  grpc::Status ReadModifyWriteRow(
      grpc::ClientContext* context,
      btproto::ReadModifyWriteRowRequest const& request,
      btproto::ReadModifyWriteRowResponse* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::ReadModifyWriteRowResponse>>
  AsyncReadModifyWriteRow(
      grpc::ClientContext* context,
      google::bigtable::v2::ReadModifyWriteRowRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientReaderInterface<btproto::ReadRowsResponse>>
  ReadRows(grpc::ClientContext* context,
           btproto::ReadRowsRequest const& request) override;

  std::unique_ptr<grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>
  AsyncReadRows(grpc::ClientContext* context,
                const google::bigtable::v2::ReadRowsRequest& request,
                grpc::CompletionQueue* cq, void* tag) override;

  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::ReadRowsResponse>>
  PrepareAsyncReadRows(::grpc::ClientContext* context,
                       const ::google::bigtable::v2::ReadRowsRequest& request,
                       ::grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>
  SampleRowKeys(grpc::ClientContext* context,
                btproto::SampleRowKeysRequest const& request) override;

  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::SampleRowKeysResponse>>
  AsyncSampleRowKeys(
      ::grpc::ClientContext* context,
      const ::google::bigtable::v2::SampleRowKeysRequest& request,
      ::grpc::CompletionQueue* cq, void* tag) override;

  std::unique_ptr<grpc::ClientReaderInterface<btproto::MutateRowsResponse>>
  MutateRows(grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request) override;

  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::MutateRowsResponse>>
  AsyncMutateRows(::grpc::ClientContext* context,
                  const ::google::bigtable::v2::MutateRowsRequest& request,
                  ::grpc::CompletionQueue* cq, void* tag) override;

  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::MutateRowsResponse>>
  PrepareAsyncMutateRows(
      ::grpc::ClientContext* context,
      const ::google::bigtable::v2::MutateRowsRequest& request,
      ::grpc::CompletionQueue* cq) override;

 private:
  ClientOptions::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return child_->BackgroundThreadsFactory();
  }

  std::shared_ptr<google::cloud::bigtable::DataClient> child_;
  google::cloud::TracingOptions tracing_options_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_DATA_CLIENT_H
