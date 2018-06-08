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

#include "google/cloud/bigtable/testing/inprocess_data_client.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

namespace btproto = google::bigtable::v2;

grpc::Status InProcessDataClient::MutateRow(
    grpc::ClientContext* context, btproto::MutateRowRequest const& request,
    btproto::MutateRowResponse* response) {
  return Stub()->MutateRow(context, request, response);
}

grpc::Status InProcessDataClient::CheckAndMutateRow(
    grpc::ClientContext* context,
    btproto::CheckAndMutateRowRequest const& request,
    btproto::CheckAndMutateRowResponse* response) {
  return Stub()->CheckAndMutateRow(context, request, response);
}

grpc::Status InProcessDataClient::ReadModifyWriteRow(
    grpc::ClientContext* context,
    btproto::ReadModifyWriteRowRequest const& request,
    btproto::ReadModifyWriteRowResponse* response) {
  return Stub()->ReadModifyWriteRow(context, request, response);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::ReadRowsResponse>>
InProcessDataClient::ReadRows(grpc::ClientContext* context,
                              btproto::ReadRowsRequest const& request) {
  return Stub()->ReadRows(context, request);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>
InProcessDataClient::SampleRowKeys(
    grpc::ClientContext* context,
    btproto::SampleRowKeysRequest const& request) {
  return Stub()->SampleRowKeys(context, request);
}

std::unique_ptr<grpc::ClientReaderInterface<btproto::MutateRowsResponse>>
InProcessDataClient::MutateRows(grpc::ClientContext* context,
                                btproto::MutateRowsRequest const& request) {
  return Stub()->MutateRows(context, request);
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
