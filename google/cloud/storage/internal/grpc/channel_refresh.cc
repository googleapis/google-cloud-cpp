// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/grpc/channel_refresh.h"
#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kRefreshPeriod = std::chrono::hours(1);

GrpcChannelRefresh::GrpcChannelRefresh(
    std::vector<std::shared_ptr<grpc::Channel>> channels)
    : channels_(std::move(channels)) {}

void GrpcChannelRefresh::StartRefreshLoop(google::cloud::CompletionQueue cq) {
  // Break the ownership cycle.
  auto wcq = std::weak_ptr<google::cloud::internal::CompletionQueueImpl>(
      google::cloud::internal::GetCompletionQueueImpl(std::move(cq)));
  for (std::size_t i = 0; i != channels_.size(); ++i) Refresh(i, wcq);
}

void GrpcChannelRefresh::Refresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq) {
  auto cq = wcq.lock();
  if (!cq) return;
  auto deadline = std::chrono::system_clock::now() + kRefreshPeriod;
  // An invalid index, stop the loop.  There is no need for synchronization
  // as the channels do not change after the class is initialized.
  if (index >= channels_.size()) return;
  if (index == 0) {
    // We create hundreds of channels in some VMs. That can create a lot of
    // noise in the logs. Logging only one channel is a good tradeoff. It shows
    // "progress" without consuming all the log output with uninteresting lines.
    GCP_LOG(INFO) << "Refreshing channel [" << index << "]";
  }
  (void)google::cloud::internal::NotifyOnStateChange::Start(
      std::move(cq), channels_.at(index), deadline)
      .then(
          [index, wcq = std::move(wcq), weak = WeakFromThis()](future<bool> f) {
            if (auto self = weak.lock()) self->OnRefresh(index, wcq, f.get());
          });
}

void GrpcChannelRefresh::OnRefresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq, bool ok) {
  // The CQ is shutting down, or the channel is shutdown, stop the loop.
  if (!ok) return;
  Refresh(index, std::move(wcq));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
