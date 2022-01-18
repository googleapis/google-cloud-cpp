// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/getenv.h"
#include "absl/memory/memory.h"
#include <google/storage/v2/storage.grpc.pb.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
DefaultStorageStub::ReadObject(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v2::ReadObjectRequest const& request) {
  using ::google::cloud::internal::StreamingReadRpcImpl;
  auto stream = grpc_stub_->ReadObject(context.get(), request);
  return absl::make_unique<
      StreamingReadRpcImpl<google::storage::v2::ReadObjectResponse>>(
      std::move(context), std::move(stream));
}

std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
DefaultStorageStub::WriteObject(std::unique_ptr<grpc::ClientContext> context) {
  using ::google::cloud::internal::StreamingWriteRpcImpl;
  using ResponseType = ::google::storage::v2::WriteObjectResponse;
  using RequestType = ::google::storage::v2::WriteObjectRequest;
  auto response = absl::make_unique<ResponseType>();
  auto stream = grpc_stub_->WriteObject(context.get(), response.get());
  return absl::make_unique<StreamingWriteRpcImpl<RequestType, ResponseType>>(
      std::move(context), std::move(response), std::move(stream));
}

StatusOr<google::storage::v2::StartResumableWriteResponse>
DefaultStorageStub::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v2::StartResumableWriteRequest const& request) {
  google::storage::v2::StartResumableWriteResponse response;
  auto status = grpc_stub_->StartResumableWrite(&context, request, &response);
  if (!status.ok()) return MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::storage::v2::QueryWriteStatusResponse>
DefaultStorageStub::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v2::QueryWriteStatusRequest const& request) {
  google::storage::v2::QueryWriteStatusResponse response;
  auto status = grpc_stub_->QueryWriteStatus(&context, request, &response);
  if (!status.ok()) return MakeStatusFromRpcError(status);
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
