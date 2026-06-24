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

#include "google/cloud/internal/rest_completion_queue_impl.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestCompletionQueueImpl::RestCompletionQueueImpl()
    : impl_(std::make_shared<RestPureCompletionQueueImpl>()) {}

void RestCompletionQueueImpl::Run() { impl_->Run(); }

void RestCompletionQueueImpl::Shutdown() { impl_->Shutdown(); }

void RestCompletionQueueImpl::CancelAll() {}

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  return impl_->MakeDeadlineTimer(deadline);
}

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeRelativeTimer(std::chrono::nanoseconds duration) {
  return impl_->MakeRelativeTimer(duration);
}

void RestCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  impl_->RunAsync(std::move(function));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
