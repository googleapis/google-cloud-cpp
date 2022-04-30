// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_round_robin.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableRoundRobin::ReadRows(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::ReadRowsRequest const& request) {
  return Child()->ReadRows(std::move(client_context), request);
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableRoundRobin::SampleRowKeys(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::SampleRowKeysRequest const& request) {
  return Child()->SampleRowKeys(std::move(client_context), request);
}

StatusOr<google::bigtable::v2::MutateRowResponse> BigtableRoundRobin::MutateRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::MutateRowRequest const& request) {
  return Child()->MutateRow(client_context, request);
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableRoundRobin::MutateRows(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::MutateRowsRequest const& request) {
  return Child()->MutateRows(std::move(client_context), request);
}

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
BigtableRoundRobin::CheckAndMutateRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  return Child()->CheckAndMutateRow(client_context, request);
}

StatusOr<google::bigtable::v2::PingAndWarmResponse>
BigtableRoundRobin::PingAndWarm(
    grpc::ClientContext& client_context,
    google::bigtable::v2::PingAndWarmRequest const& request) {
  return Child()->PingAndWarm(client_context, request);
}

StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>
BigtableRoundRobin::ReadModifyWriteRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  return Child()->ReadModifyWriteRow(client_context, request);
}

future<StatusOr<google::bigtable::v2::MutateRowResponse>>
BigtableRoundRobin::AsyncMutateRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::MutateRowRequest const& request) {
  return Child()->AsyncMutateRow(cq, std::move(context), request);
}

future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
BigtableRoundRobin::AsyncCheckAndMutateRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  return Child()->AsyncCheckAndMutateRow(cq, std::move(context), request);
}

future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
BigtableRoundRobin::AsyncReadModifyWriteRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  return Child()->AsyncReadModifyWriteRow(cq, std::move(context), request);
}

std::shared_ptr<BigtableStub> BigtableRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
