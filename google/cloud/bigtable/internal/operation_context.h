// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_H

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A Bigtable-specific context that persists across retries.
 *
 * The client communicates with the server via metadata, prefixed with
 * "x-goog-cbt-cookie". This helps the server associate RPCs with a single
 * client call. This information can be used to make routing decisions, for
 * example, to avoid outages.
 *
 * The lifetime for this object should be a single client call.
 *
 * @code
 * Result Connection::Foo() {
 *   OperationContext operation_context;
 *   auto result = RetryLoop(...,
 *     [operation_context, &stub] (auto& context, auto const& request) {
 *       operation_context.PreCall(context);
 *       auto result = stub.Foo(context, request);
 *       operation_context.PostCall(context);
 *       return result;
 *     }, ...);
 *   operation_context.OnDone(result.status());
 *   return result;
 * }
 * @endcode
 */
class OperationContext {
 public:
  // TODO : remove when all RPCs are instrumented.
  OperationContext() = default;

  explicit OperationContext(
      std::vector<std::shared_ptr<Metric>> stub_applicable_metrics);

  // Called before each RPC attempt.
  void PreCall(grpc::ClientContext& context);
  // Called after receiving RPC attempt response.
  void PostCall(grpc::ClientContext const& context,
                google::cloud::Status const& status);
  // A hook that executes at the end of a client operation.
  void OnDone(Status const& status);
  // Called for some RPCs. Definition of "first response" may vary by RPC.
  void FirstResponse(grpc::ClientContext const& context);

 private:
  void ProcessMetadata(
      std::multimap<grpc::string_ref, grpc::string_ref> const& metadata);

  std::unordered_map<std::string, std::string> cookies_;
  int attempt_number_ = 0;
  std::chrono::system_clock::time_point operation_start_ =
      std::chrono::system_clock::now();
  std::chrono::system_clock::time_point attempt_start_;

  // We call stub method specific factory functions that
  // populate the metrics that are supported on that stub method.
  // These metrics share a common interface that to record data analogous to
  // PreCall, PostCall, OnDone, etc. When the OperationContext method is called
  // it iterates through the metrics calling that function on the
  // MetricInterface.
  std::vector<std::shared_ptr<Metric>> stub_applicable_metrics_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_H
