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

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/discoveryengine/v1/rank_service.proto

#include "google/cloud/discoveryengine/v1/internal/rank_tracing_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/traced_stream_range.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace discoveryengine_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

RankServiceTracingConnection::RankServiceTracingConnection(
    std::shared_ptr<discoveryengine_v1::RankServiceConnection> child)
    : child_(std::move(child)) {}

StatusOr<google::cloud::discoveryengine::v1::RankResponse>
RankServiceTracingConnection::Rank(
    google::cloud::discoveryengine::v1::RankRequest const& request) {
  auto span =
      internal::MakeSpan("discoveryengine_v1::RankServiceConnection::Rank");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->Rank(request));
}

StreamRange<google::longrunning::Operation>
RankServiceTracingConnection::ListOperations(
    google::longrunning::ListOperationsRequest request) {
  auto span = internal::MakeSpan(
      "discoveryengine_v1::RankServiceConnection::ListOperations");
  internal::OTelScope scope(span);
  auto sr = child_->ListOperations(std::move(request));
  return internal::MakeTracedStreamRange<google::longrunning::Operation>(
      std::move(span), std::move(sr));
}

StatusOr<google::longrunning::Operation>
RankServiceTracingConnection::GetOperation(
    google::longrunning::GetOperationRequest const& request) {
  auto span = internal::MakeSpan(
      "discoveryengine_v1::RankServiceConnection::GetOperation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetOperation(request));
}

Status RankServiceTracingConnection::CancelOperation(
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpan(
      "discoveryengine_v1::RankServiceConnection::CancelOperation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->CancelOperation(request));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<discoveryengine_v1::RankServiceConnection>
MakeRankServiceTracingConnection(
    std::shared_ptr<discoveryengine_v1::RankServiceConnection> conn) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (internal::TracingEnabled(conn->options())) {
    conn = std::make_shared<RankServiceTracingConnection>(std::move(conn));
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return conn;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace discoveryengine_v1_internal
}  // namespace cloud
}  // namespace google
