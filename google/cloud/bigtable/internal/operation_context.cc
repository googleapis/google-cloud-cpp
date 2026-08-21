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

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/internal/metrics.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include <opentelemetry/context/runtime_context.h>
#endif
#include <string_view>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void OperationContext::ProcessMetadata(
    std::multimap<grpc::string_ref, grpc::string_ref> const& metadata) {
  for (auto const& [k, v] : metadata) {
    std::string_view key_view{k.data(), k.size()};
    if (absl::StartsWith(key_view, "x-goog-cbt-cookie")) {
      cookies_[std::string{key_view}] = std::string{v.data(), v.size()};
    }
  }
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

OperationContext::OperationContext(std::vector<std::shared_ptr<Metric>> metrics,
                                   std::shared_ptr<Clock> clock)
    : cloned_metrics_(std::move(metrics)), clock_(std::move(clock)) {}

void OperationContext::StubSelection(StubSelectionParams const& params) {
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : cloned_metrics_) {
    m->StubSelection(otel_context, params);
  }
}

void OperationContext::PreCall(grpc::ClientContext& client_context) {
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto attempt_start = clock_->Now();
  if (attempt_number_ == 0) {
    operation_start_ = attempt_start;
  }
  for (auto& m : cloned_metrics_) {
    m->PreCall(otel_context,
               PreCallParams{attempt_start, attempt_number_ == 0});
  }

  for (auto const& [key, value] : cookies_) {
    client_context.AddMetadata(key, value);
  }
  client_context.AddMetadata("bigtable-attempt",
                             std::to_string(attempt_number_++));
}

void OperationContext::PostCall(grpc::ClientContext const& client_context,
                                google::cloud::Status const& status) {
  ProcessMetadata(client_context.GetServerInitialMetadata());
  ProcessMetadata(client_context.GetServerTrailingMetadata());
  auto attempt_end = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : cloned_metrics_) {
    m->PostCall(otel_context, client_context,
                PostCallParams{attempt_end, status});
  }
}

void OperationContext::OnDone(Status const& s) {
  auto operation_end = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : cloned_metrics_) {
    m->OnDone(otel_context, OnDoneParams{operation_end, s});
  }
}

void OperationContext::ElementRequest(grpc::ClientContext const&) {
  auto element_request = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : cloned_metrics_) {
    m->ElementRequest(otel_context, ElementRequestParams{element_request});
  }
}

void OperationContext::ElementDelivery(grpc::ClientContext const&) {
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto first_response = clock_->Now();
  for (auto& m : cloned_metrics_) {
    m->ElementDelivery(otel_context,
                       ElementDeliveryParams{first_response, first_response_});
  }
  if (first_response_) {
    first_response_ = false;
  }
}

#else  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

OperationContext::OperationContext(std::vector<std::shared_ptr<Metric>>,
                                   std::shared_ptr<Clock>) {}

void OperationContext::PreCall(grpc::ClientContext& client_context) {
  for (auto const& [key, value] : cookies_) {
    client_context.AddMetadata(key, value);
  }
  client_context.AddMetadata("bigtable-attempt",
                             std::to_string(attempt_number_++));
}

void OperationContext::PostCall(grpc::ClientContext const& client_context,
                                google::cloud::Status const&) {
  ProcessMetadata(client_context.GetServerInitialMetadata());
  ProcessMetadata(client_context.GetServerTrailingMetadata());
}

void OperationContext::OnDone(Status const&) {}

void OperationContext::ElementRequest(grpc::ClientContext const&) {}

void OperationContext::ElementDelivery(grpc::ClientContext const&) {}

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
