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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_REFRESH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_REFRESH_H

#include "google/cloud/storage/version.h"
#include "google/cloud/completion_queue.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class GrpcChannelRefresh
    : public std::enable_shared_from_this<GrpcChannelRefresh> {
 public:
  explicit GrpcChannelRefresh(
      std::vector<std::shared_ptr<grpc::Channel>> channels);
  ~GrpcChannelRefresh() = default;

  void StartRefreshLoop(google::cloud::CompletionQueue cq);

  std::vector<std::shared_ptr<grpc::Channel>> channels() const {
    return channels_;
  }

 private:
  std::weak_ptr<GrpcChannelRefresh> WeakFromThis() {
    return shared_from_this();
  }

  void Refresh(std::size_t index,
               std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq);

  void OnRefresh(
      std::size_t index,
      std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq, bool ok);

  std::vector<std::shared_ptr<grpc::Channel>> channels_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_REFRESH_H
