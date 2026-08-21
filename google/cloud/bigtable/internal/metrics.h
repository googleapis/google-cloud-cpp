// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>
#include <opentelemetry/context/context.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

enum class ChannelPoolLbPolicy {
  kRoundRobin,
  kRandomTwoLeastUsed,
};

enum class TransportType {
  kCloudPath,
  kDirectPath,
};

enum class RpcType {
  kUnary,
  kStreaming,
};

struct StubSelectionParams {
  std::int64_t outstanding_rpcs;
  ChannelPoolLbPolicy channel_pool_lb_policy;
  TransportType transport_type;
  RpcType streaming;
};

struct PreCallParams {
  OperationContext::Clock::time_point attempt_start;
  bool first_attempt;
};

struct PostCallParams {
  OperationContext::Clock::time_point attempt_end;
  google::cloud::Status attempt_status;
};

struct OnDoneParams {
  OperationContext::Clock::time_point operation_end;
  google::cloud::Status operation_status;
};

struct ElementRequestParams {
  OperationContext::Clock::time_point element_request;
};

struct ElementDeliveryParams {
  OperationContext::Clock::time_point element_delivery;
  bool first_response;
};

enum class MetricSchema {
  kTable,
  kClient,
};

class Metric {
 public:
  using LatencyDuration = std::chrono::duration<double, std::milli>;

  virtual ~Metric() = 0;
  virtual MetricSchema schema() const = 0;
  virtual void StubSelection(opentelemetry::context::Context const&,
                             StubSelectionParams const&) {}
  virtual void PreCall(opentelemetry::context::Context const&,
                       PreCallParams const&) {}
  virtual void PostCall(opentelemetry::context::Context const&,
                        grpc::ClientContext const&, PostCallParams const&) {}
  virtual void OnDone(opentelemetry::context::Context const&,
                      OnDoneParams const&) {}
  virtual void ElementRequest(opentelemetry::context::Context const&,
                              ElementRequestParams const&) {}
  virtual void ElementDelivery(opentelemetry::context::Context const&,
                               ElementDeliveryParams const&) {}
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
