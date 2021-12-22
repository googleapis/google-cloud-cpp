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

#include "google/cloud/storage/internal/storage_auth.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::unique_ptr<StorageStub::ReadObjectStream> StorageAuth::ReadObject(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v2::ReadObjectRequest const& request) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::storage::v2::ReadObjectResponse>;
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return absl::make_unique<ErrorStream>(std::move(status));
  return child_->ReadObject(std::move(context), request);
}

std::unique_ptr<StorageStub::WriteObjectStream> StorageAuth::WriteObject(
    std::unique_ptr<grpc::ClientContext> context) {
  using ErrorStream = ::google::cloud::internal::StreamingWriteRpcError<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return absl::make_unique<ErrorStream>(std::move(status));
  return child_->WriteObject(std::move(context));
}

StatusOr<google::storage::v2::StartResumableWriteResponse>
StorageAuth::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v2::StartResumableWriteRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->StartResumableWrite(context, request);
}

StatusOr<google::storage::v2::QueryWriteStatusResponse>
StorageAuth::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v2::QueryWriteStatusRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->QueryWriteStatus(context, request);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
