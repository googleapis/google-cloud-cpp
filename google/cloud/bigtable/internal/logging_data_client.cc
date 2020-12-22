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

#include "google/cloud/bigtable/internal/logging_data_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/log.h"
#include "google/cloud/tracing_options.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btproto = google::bigtable::v2;
using ::google::cloud::internal::LogWrapper;

grpc::Status LoggingDataClient::MutateRow(
    grpc::ClientContext* context, btproto::MutateRowRequest const& request,
    btproto::MutateRowResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::MutateRowRequest const& request,
             btproto::MutateRowResponse* response) {
        return child_->MutateRow(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>
LoggingDataClient::AsyncMutateRow(grpc::ClientContext* context,
                                  btproto::MutateRowRequest const& request,
                                  grpc::CompletionQueue* cq) {
  return child_->AsyncMutateRow(context, request, cq);
}

grpc::Status LoggingDataClient::CheckAndMutateRow(
    grpc::ClientContext* context,
    btproto::CheckAndMutateRowRequest const& request,
    btproto::CheckAndMutateRowResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::CheckAndMutateRowRequest const& request,
             btproto::CheckAndMutateRowResponse* response) {
        return child_->CheckAndMutateRow(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::v2::CheckAndMutateRowResponse>>
LoggingDataClient::AsyncCheckAndMutateRow(
    grpc::ClientContext* context,
    const google::bigtable::v2::CheckAndMutateRowRequest& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCheckAndMutateRow(context, request, cq);
}

grpc::Status LoggingDataClient::ReadModifyWriteRow(
    grpc::ClientContext* context,
    btproto::ReadModifyWriteRowRequest const& request,
    btproto::ReadModifyWriteRowResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::ReadModifyWriteRowRequest const& request,
             btproto::ReadModifyWriteRowResponse* response) {
        return child_->ReadModifyWriteRow(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::v2::ReadModifyWriteRowResponse>>
LoggingDataClient::AsyncReadModifyWriteRow(
    grpc::ClientContext* context,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncReadModifyWriteRow(context, request, cq);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::ReadRowsResponse>>
LoggingDataClient::ReadRows(grpc::ClientContext* context,
                            btproto::ReadRowsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::ReadRowsRequest const& request) {
        return child_->ReadRows(context, request);
      },
      context, request, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>
LoggingDataClient::AsyncReadRows(
    grpc::ClientContext* context,
    const google::bigtable::v2::ReadRowsRequest& request,
    grpc::CompletionQueue* cq, void* tag) {
  return child_->AsyncReadRows(context, request, cq, tag);
}

std::unique_ptr<::grpc::ClientAsyncReaderInterface<
    ::google::bigtable::v2::ReadRowsResponse>>
LoggingDataClient::PrepareAsyncReadRows(
    ::grpc::ClientContext* context,
    const ::google::bigtable::v2::ReadRowsRequest& request,
    ::grpc::CompletionQueue* cq) {
  return child_->PrepareAsyncReadRows(context, request, cq);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>
LoggingDataClient::SampleRowKeys(grpc::ClientContext* context,
                                 btproto::SampleRowKeysRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::SampleRowKeysRequest const& request) {
        return child_->SampleRowKeys(context, request);
      },
      context, request, __func__, tracing_options_);
}

std::unique_ptr<::grpc::ClientAsyncReaderInterface<
    ::google::bigtable::v2::SampleRowKeysResponse>>
LoggingDataClient::AsyncSampleRowKeys(
    ::grpc::ClientContext* context,
    const ::google::bigtable::v2::SampleRowKeysRequest& request,
    ::grpc::CompletionQueue* cq, void* tag) {
  return child_->AsyncSampleRowKeys(context, request, cq, tag);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::MutateRowsResponse>>
LoggingDataClient::MutateRows(grpc::ClientContext* context,
                              btproto::MutateRowsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request) {
        return child_->MutateRows(context, request);
      },
      context, request, __func__, tracing_options_);
}

std::unique_ptr<::grpc::ClientAsyncReaderInterface<
    ::google::bigtable::v2::MutateRowsResponse>>
LoggingDataClient::AsyncMutateRows(
    ::grpc::ClientContext* context,
    const ::google::bigtable::v2::MutateRowsRequest& request,
    ::grpc::CompletionQueue* cq, void* tag) {
  return child_->AsyncMutateRows(context, request, cq, tag);
}

std::unique_ptr<::grpc::ClientAsyncReaderInterface<
    ::google::bigtable::v2::MutateRowsResponse>>
LoggingDataClient::PrepareAsyncMutateRows(
    ::grpc::ClientContext* context,
    const ::google::bigtable::v2::MutateRowsRequest& request,
    ::grpc::CompletionQueue* cq) {
  return child_->PrepareAsyncMutateRows(context, request, cq);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
