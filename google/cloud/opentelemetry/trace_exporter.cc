// Copyright 2023 Google LLC
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

#include "google/cloud/opentelemetry/trace_exporter.h"
#include "google/cloud/trace/v2/trace_client.h"

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class TraceExporter final : public opentelemetry::sdk::trace::SpanExporter {
 public:
  explicit TraceExporter(Project project,
                         std::shared_ptr<trace_v2::TraceServiceConnection> conn)
      : project_(std::move(project)), client_(std::move(conn)) {}

  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override {
    return std::make_unique<otel_internal::Recordable>(project_);
  }

  opentelemetry::sdk::common::ExportResult Export(
      opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>> const&
          spans) noexcept override {
    google::devtools::cloudtrace::v2::BatchWriteSpansRequest request;
    request.set_name(project_.FullName());
    for (auto& recordable : spans) {
      auto span = std::unique_ptr<otel_internal::Recordable>(
          static_cast<otel_internal::Recordable*>(recordable.release()));
      *request.add_spans() = std::move(*span).as_proto();
    }

    auto status = client_.BatchWriteSpans(request);
    return status.ok() ? opentelemetry::sdk::common::ExportResult::kSuccess
                       : opentelemetry::sdk::common::ExportResult::kFailure;
  }

  bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

 private:
  Project project_;
  trace_v2::TraceServiceClient client_;
};

}  // namespace

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> MakeTraceExporter(
    Project project, Options options) {
  // TODO(#11156) - We should filter out options that enable tracing. We should
  // not trace requests initiated by the underlying trace_v2 client.
  return std::make_unique<TraceExporter>(
      std::move(project),
      trace_v2::MakeTraceServiceConnection(std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> MakeTraceExporter(
    Project project, std::shared_ptr<trace_v2::TraceServiceConnection> conn) {
  return std::make_unique<otel::TraceExporter>(std::move(project),
                                               std::move(conn));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
