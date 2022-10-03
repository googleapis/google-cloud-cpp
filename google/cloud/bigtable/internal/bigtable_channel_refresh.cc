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

#include "google/cloud/bigtable/internal/bigtable_channel_refresh.h"
#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kRefreshPeriod = std::chrono::hours(1);

std::shared_ptr<BigtableChannelRefresh> BigtableChannelRefresh::Create(
    std::shared_ptr<BigtableStub> child, CompletionQueue cq,
    std::vector<std::shared_ptr<grpc::Channel>> channels) {
  auto stub = std::shared_ptr<BigtableChannelRefresh>(
      new BigtableChannelRefresh(std::move(child), std::move(channels)));
  stub->StartRefreshLoop(std::move(cq));
  return stub;
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableChannelRefresh::ReadRows(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::ReadRowsRequest const& request) {
  return child_->ReadRows(std::move(client_context), request);
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableChannelRefresh::SampleRowKeys(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::SampleRowKeysRequest const& request) {
  return child_->SampleRowKeys(std::move(client_context), request);
}

StatusOr<google::bigtable::v2::MutateRowResponse>
BigtableChannelRefresh::MutateRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::MutateRowRequest const& request) {
  return child_->MutateRow(client_context, request);
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableChannelRefresh::MutateRows(
    std::unique_ptr<grpc::ClientContext> client_context,
    google::bigtable::v2::MutateRowsRequest const& request) {
  return child_->MutateRows(std::move(client_context), request);
}

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
BigtableChannelRefresh::CheckAndMutateRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  return child_->CheckAndMutateRow(client_context, request);
}

StatusOr<google::bigtable::v2::PingAndWarmResponse>
BigtableChannelRefresh::PingAndWarm(
    grpc::ClientContext& client_context,
    google::bigtable::v2::PingAndWarmRequest const& request) {
  return child_->PingAndWarm(client_context, request);
}

StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>
BigtableChannelRefresh::ReadModifyWriteRow(
    grpc::ClientContext& client_context,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  return child_->ReadModifyWriteRow(client_context, request);
}

std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::ReadRowsResponse>>
BigtableChannelRefresh::AsyncReadRows(
    google::cloud::CompletionQueue const& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::ReadRowsRequest const& request) {
  return child_->AsyncReadRows(cq, std::move(context), request);
}

std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::SampleRowKeysResponse>>
BigtableChannelRefresh::AsyncSampleRowKeys(
    google::cloud::CompletionQueue const& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::SampleRowKeysRequest const& request) {
  return child_->AsyncSampleRowKeys(cq, std::move(context), request);
}

future<StatusOr<google::bigtable::v2::MutateRowResponse>>
BigtableChannelRefresh::AsyncMutateRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::MutateRowRequest const& request) {
  return child_->AsyncMutateRow(cq, std::move(context), request);
}

std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
    google::bigtable::v2::MutateRowsResponse>>
BigtableChannelRefresh::AsyncMutateRows(
    google::cloud::CompletionQueue const& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::MutateRowsRequest const& request) {
  return child_->AsyncMutateRows(cq, std::move(context), request);
}

future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
BigtableChannelRefresh::AsyncCheckAndMutateRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  return child_->AsyncCheckAndMutateRow(cq, std::move(context), request);
}

future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
BigtableChannelRefresh::AsyncReadModifyWriteRow(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
  return child_->AsyncReadModifyWriteRow(cq, std::move(context), request);
}

void BigtableChannelRefresh::Refresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq) {
  auto cq = wcq.lock();
  if (!cq) return;
  auto deadline = std::chrono::system_clock::now() + kRefreshPeriod;
  GCP_LOG(INFO) << "Refreshing channel [" << index << "]";
  (void)internal::NotifyOnStateChange::Start(std::move(cq), channels_.at(index),
                                             deadline)
      .then(
          [index, wcq = std::move(wcq), weak = WeakFromThis()](future<bool> f) {
            if (auto self = weak.lock()) self->OnRefresh(index, wcq, f.get());
          });
}

void BigtableChannelRefresh::OnRefresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq, bool ok) {
  // The CQ is shutting down, or the channel is shutdown, stop the loop.
  if (!ok) return;
  Refresh(index, std::move(wcq));
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void BigtableChannelRefresh::StartRefreshLoop(CompletionQueue cq) {
  // Break the ownership cycle.
  auto wcq = std::weak_ptr<internal::CompletionQueueImpl>(
      internal::GetCompletionQueueImpl(cq));
  for (std::size_t i = 0; i != channels_.size(); ++i) Refresh(i, wcq);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
