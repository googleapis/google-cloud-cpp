// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_auth.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::unique_ptr<StorageStub::ObjectMediaStream> StorageAuth::GetObjectMedia(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v1::GetObjectMediaRequest const& request) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::storage::v1::GetObjectMediaResponse>;
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return absl::make_unique<ErrorStream>(std::move(status));
  return child_->GetObjectMedia(std::move(context), request);
}

std::unique_ptr<StorageStub::InsertStream> StorageAuth::InsertObjectMedia(
    std::unique_ptr<grpc::ClientContext> context) {
  using ErrorStream = ::google::cloud::internal::StreamingWriteRpcError<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return absl::make_unique<ErrorStream>(std::move(status));
  return child_->InsertObjectMedia(std::move(context));
}

StatusOr<google::storage::v1::StartResumableWriteResponse>
StorageAuth::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v1::StartResumableWriteRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->StartResumableWrite(context, request);
}

StatusOr<google::storage::v1::QueryWriteStatusResponse>
StorageAuth::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v1::QueryWriteStatusRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->QueryWriteStatus(context, request);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
