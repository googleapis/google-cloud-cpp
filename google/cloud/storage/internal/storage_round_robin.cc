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

#include "google/cloud/storage/internal/storage_round_robin.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::unique_ptr<StorageStub::ObjectMediaStream>
StorageRoundRobin::GetObjectMedia(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v1::GetObjectMediaRequest const& request) {
  return Child()->GetObjectMedia(std::move(context), request);
}

std::unique_ptr<StorageStub::InsertStream> StorageRoundRobin::InsertObjectMedia(
    std::unique_ptr<grpc::ClientContext> context) {
  return Child()->InsertObjectMedia(std::move(context));
}

StatusOr<google::storage::v1::StartResumableWriteResponse>
StorageRoundRobin::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v1::StartResumableWriteRequest const& request) {
  return Child()->StartResumableWrite(context, request);
}

StatusOr<google::storage::v1::QueryWriteStatusResponse>
StorageRoundRobin::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v1::QueryWriteStatusRequest const& request) {
  return Child()->QueryWriteStatus(context, request);
}

std::shared_ptr<StorageStub> StorageRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
