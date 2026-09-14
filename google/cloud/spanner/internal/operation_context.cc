// Copyright 2026 Google LLC
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

#include "google/cloud/spanner/internal/operation_context.h"
#include "google/cloud/spanner/internal/spanner_request_id.h"
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

OperationContext::OperationContext(
    std::shared_ptr<std::string const> static_prefix,
    std::uint64_t request_index, std::string_view rpc_name)
    : static_prefix_(std::move(static_prefix)),
      request_index_(request_index),
      rpc_name_(rpc_name) {}

OperationContext::OperationContext(OperationContext&& other) noexcept {
  std::scoped_lock lock(other.mu_);
  static_prefix_ = std::move(other.static_prefix_);
  request_index_ = other.request_index_;
  rpc_name_ = other.rpc_name_;
  channel_id_ = other.channel_id_;
  attempt_index_ = other.attempt_index_;
  current_request_id_ = std::move(other.current_request_id_);
}

OperationContext& OperationContext::operator=(
    OperationContext&& other) noexcept {
  if (this != &other) {
    std::scoped_lock lock(mu_, other.mu_);
    static_prefix_ = std::move(other.static_prefix_);
    request_index_ = other.request_index_;
    rpc_name_ = other.rpc_name_;
    channel_id_ = other.channel_id_;
    attempt_index_ = other.attempt_index_;
    current_request_id_ = std::move(other.current_request_id_);
  }
  return *this;
}

void OperationContext::BindChannel(std::uint32_t channel_id) {
  std::scoped_lock lock(mu_);
  channel_id_ = channel_id;
}

void OperationContext::PreCall(grpc::ClientContext& client_context) {
  std::scoped_lock lock(mu_);
  ++attempt_index_;
  if (static_prefix_ != nullptr) {
    current_request_id_ = FormatSpannerRequestId(
        *static_prefix_, channel_id_, request_index_, attempt_index_);
    client_context.AddMetadata("x-goog-spanner-request-id",
                               current_request_id_);
  }
}

void OperationContext::PostCall(grpc::ClientContext const&, Status const&) {
  // Hook for metrics / debugging
}

void OperationContext::OnDone(Status const&) {
  // Hook for metrics / latencies
}

std::optional<std::string> OperationContext::RequestId() const {
  std::scoped_lock lock(mu_);
  if (current_request_id_.empty()) return std::nullopt;
  return current_request_id_;
}

std::uint64_t OperationContext::request_index() const { return request_index_; }

std::uint32_t OperationContext::attempt_index() const {
  std::scoped_lock lock(mu_);
  return attempt_index_;
}

std::uint32_t OperationContext::channel_id() const {
  std::scoped_lock lock(mu_);
  return channel_id_;
}

std::string_view OperationContext::rpc_name() const { return rpc_name_; }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
