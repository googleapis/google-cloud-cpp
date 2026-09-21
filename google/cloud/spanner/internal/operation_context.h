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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_OPERATION_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_OPERATION_CONTEXT_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/operation_context.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class OperationContext : public google::cloud::internal::OperationContext {
 public:
  OperationContext();
  OperationContext(std::shared_ptr<std::string const> static_prefix,
                   std::uint64_t request_index, std::string_view rpc_name);

  // Move operations transfer context state across threads.
  OperationContext(OperationContext&& other) noexcept;
  OperationContext& operator=(OperationContext&& other) noexcept;

  OperationContext(OperationContext const&) = delete;
  OperationContext& operator=(OperationContext const&) = delete;

  // Binds the physical gRPC channel index (0..N-1) for subsequent attempts.
  void BindChannel(std::uint32_t channel_id);

  // Advances attempt_index_, formats header, and injects
  // "x-goog-spanner-request-id" into context.
  void PreCall(grpc::ClientContext& client_context) override;

  // Called immediately after an attempt returns (hook for metrics / debugging).
  void PostCall(grpc::ClientContext const& client_context,
                Status const& status) override;

  // Called when the overall logical operation completes (hook for metrics /
  // latencies).
  void OnDone(Status const& status) override;

  // Returns the active formatted request ID string for the current attempt.
  std::optional<std::string> RequestId() const;

  std::uint64_t request_index() const;
  std::uint32_t attempt_index() const;
  std::uint32_t channel_id() const;
  std::string_view rpc_name() const;

 private:
  std::shared_ptr<std::string const> static_prefix_;
  std::uint64_t request_index_;
  std::string_view rpc_name_;
  mutable std::mutex mu_;
  std::uint32_t channel_id_ = 0;
  std::uint32_t attempt_index_ = 0;
  std::string current_request_id_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_OPERATION_CONTEXT_H
