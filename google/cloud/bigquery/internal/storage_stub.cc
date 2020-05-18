// Copyright 2019 Google LLC
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

#include "google/cloud/bigquery/internal/storage_stub.h"
#include "google/cloud/bigquery/connection.h"
#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/internal/stream_reader.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include <google/cloud/bigquery/storage/v1beta1/storage.grpc.pb.h>
#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <grpcpp/create_channel.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

namespace {

constexpr auto kRoutingHeader = "x-goog-request-params";

namespace bigquerystorage_proto = ::google::cloud::bigquery::storage::v1beta1;

using ::google::cloud::MakeStatusFromRpcError;
using ::google::cloud::optional;
using ::google::cloud::StatusOr;

// An implementation of StreamReader for gRPC unary-streaming methods.
template <class T>
class GrpcStreamReader : public StreamReader<T> {
 public:
  GrpcStreamReader(std::unique_ptr<grpc::ClientContext> context,
                   std::unique_ptr<grpc::ClientReaderInterface<T>> reader)
      : context_(std::move(context)), reader_(std::move(reader)) {}

  StatusOr<optional<T>> NextValue() override {
    T t;
    if (reader_->Read(&t)) {
      return optional<T>(t);
    }
    grpc::Status grpc_status = reader_->Finish();
    if (!grpc_status.ok()) {
      return MakeStatusFromRpcError(grpc_status);
    }
    return optional<T>();
  }

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<grpc::ClientReaderInterface<T>> reader_;
};

class DefaultStorageStub : public StorageStub {
 public:
  explicit DefaultStorageStub(
      std::unique_ptr<bigquerystorage_proto::BigQueryStorage::StubInterface>
          grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  google::cloud::StatusOr<bigquerystorage_proto::ReadSession> CreateReadSession(
      bigquerystorage_proto::CreateReadSessionRequest const& request) override;

  std::unique_ptr<StreamReader<bigquerystorage_proto::ReadRowsResponse>>
  ReadRows(bigquerystorage_proto::ReadRowsRequest const& request) override;

 private:
  std::unique_ptr<bigquerystorage_proto::BigQueryStorage::StubInterface>
      grpc_stub_;
};

google::cloud::StatusOr<bigquerystorage_proto::ReadSession>
DefaultStorageStub::CreateReadSession(
    bigquerystorage_proto::CreateReadSessionRequest const& request) {
  bigquerystorage_proto::ReadSession response;
  grpc::ClientContext client_context;

  // For performance reasons, the Google routing layer does not parse
  // request messages. As such, we must hoist the values required for
  // routing into a header.
  //
  // TODO(aryann): Replace the below string concatenations with
  // absl::Substitute.
  //
  // TODO(aryann): URL escape the project ID and dataset ID before
  // placing them into the routing header.
  std::string routing_header = "table_reference.project_id=";
  routing_header += request.table_reference().project_id();
  routing_header += "&table_reference.dataset_id=";
  routing_header += request.table_reference().dataset_id();
  client_context.AddMetadata(kRoutingHeader, routing_header);

  grpc::Status grpc_status =
      grpc_stub_->CreateReadSession(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

std::unique_ptr<StreamReader<bigquerystorage_proto::ReadRowsResponse>>
DefaultStorageStub::ReadRows(
    bigquerystorage_proto::ReadRowsRequest const& request) {
  // TODO(aryann): Replace this with `absl::make_unique`.
  auto client_context =
      std::unique_ptr<grpc::ClientContext>(new grpc::ClientContext);

  // TODO(aryann): Replace the below string concatenations with
  // absl::Substitute.
  //
  // TODO(aryann): URL escape the project ID and dataset ID before
  // placing them into the routing header.
  std::string routing_header = "read_position.stream.name=";
  routing_header += request.read_position().stream().name();
  client_context->AddMetadata(kRoutingHeader, routing_header);

  auto stream = grpc_stub_->ReadRows(client_context.get(), request);
  // TODO(aryann): Replace this with `absl::make_unique`.
  return std::unique_ptr<StreamReader<bigquerystorage_proto::ReadRowsResponse>>(
      new GrpcStreamReader<bigquerystorage_proto::ReadRowsResponse>(
          std::move(client_context), std::move(stream)));
}

}  // namespace

std::shared_ptr<StorageStub> MakeDefaultStorageStub(
    ConnectionOptions const& options) {
  auto grpc_stub =
      bigquerystorage_proto::BigQueryStorage::NewStub(grpc::CreateCustomChannel(
          options.bigquerystorage_endpoint(), options.credentials(),
          options.CreateChannelArguments()));

  return std::make_shared<DefaultStorageStub>(std::move(grpc_stub));
}

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
