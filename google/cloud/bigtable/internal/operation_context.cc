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

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {
std::vector<std::shared_ptr<Metric>> CloneMetrics(
    ResourceLabels const& resource_labels, DataLabels const& data_labels,
    std::vector<std::shared_ptr<Metric const>> const& metrics) {
  std::vector<std::shared_ptr<Metric>> v;
  for (auto const& m : metrics) {
    v.emplace_back(m->clone(resource_labels, data_labels));
  }
  return v;
}

}  // namespace

OperationContext::OperationContext(
    ResourceLabels const& resource_labels, DataLabels const& data_labels,
    std::vector<std::shared_ptr<Metric const>> const& stub_specific_metrics,
    std::shared_ptr<Clock> clock)
    : stub_specific_metrics_(
          CloneMetrics(resource_labels, data_labels, stub_specific_metrics)),
      clock_(clock) {}

void OperationContext::PreCall(grpc::ClientContext& context) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto attempt_start = clock_->Now();
  for (auto& m : stub_specific_metrics_) {
    m->PreCall(otel_context, PreCallParams{attempt_start});
  }
#endif

  for (auto const& h : cookies_) {
    context.AddMetadata(h.first, h.second);
  }
  context.AddMetadata("bigtable-attempt", std::to_string(attempt_number_++));
}

void OperationContext::PostCall(grpc::ClientContext const& context,
                                google::cloud::Status const& status) {
  ProcessMetadata(context.GetServerInitialMetadata());
  ProcessMetadata(context.GetServerTrailingMetadata());
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto attempt_end = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : stub_specific_metrics_) {
    m->PostCall(otel_context, context.GetServerInitialMetadata(),
                context.GetServerTrailingMetadata(),
                PostCallParams{attempt_end, status});
  }
#endif
}

void OperationContext::OnDone(Status const& s) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto operation_end = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : stub_specific_metrics_) {
    m->OnDone(otel_context, OnDoneParams{operation_end, s});
  }
#endif
}

void OperationContext::ElementRequest(grpc::ClientContext const& context) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto element_request = clock_->Now();
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : stub_specific_metrics_) {
    m->ElementRequest(otel_context, ElementRequestParams{element_request});
  }
#endif
}

void OperationContext::ElementDelivery(grpc::ClientContext const&) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto first_response = clock_->Now();
  for (auto& m : stub_specific_metrics_) {
    m->ElementDelivery(otel_context,
                       ElementDeliveryParams{first_response, first_response_});
  }
  if (first_response_) {
    first_response_ = false;
  }

#endif
}

void OperationContext::ProcessMetadata(
    std::multimap<grpc::string_ref, grpc::string_ref> const& metadata) {
  for (auto const& kv : metadata) {
    auto key = std::string{kv.first.data(), kv.first.size()};
    if (absl::StartsWith(key, "x-goog-cbt-cookie")) {
      cookies_[std::move(key)] =
          std::string{kv.second.data(), kv.second.size()};
    }
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
