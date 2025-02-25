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
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include <memory>
#include <mutex>
#include <random>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class ThreadSafeGenerator {
 public:
  ThreadSafeGenerator() : generator_(internal::MakeDefaultPRNG()) {}

  auto generate(int size) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::uniform_int_distribution<int>(0, size - 1)(generator_);
  }

 private:
  std::mutex mu_;
  internal::DefaultPRNG generator_;
};

class TraceExporter final : public opentelemetry::sdk::trace::SpanExporter {
 public:
  explicit TraceExporter(Project project,
                         std::shared_ptr<trace_v2::TraceServiceConnection> conn)
      : project_(std::move(project)),
        client_(std::move(conn)),
        generator_([state = std::make_shared<ThreadSafeGenerator>()](int size) {
          return state->generate(size);
        }) {}

  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override {
    auto recordable = internal::NoExceptAction<
        std::unique_ptr<opentelemetry::sdk::trace::Recordable>>([&] {
      return std::make_unique<otel_internal::Recordable>(project_, generator_);
    });
    if (recordable) return *std::move(recordable);
    GCP_LOG(WARNING) << "Exception thrown while creating span.";
    return nullptr;
  }

  opentelemetry::sdk::common::ExportResult Export(
      opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>> const&
          spans) noexcept override {
    auto result =
        internal::NoExceptAction<opentelemetry::sdk::common::ExportResult>(
            [&] { return ExportImpl(spans); });
    if (result) return *std::move(result);
    GCP_LOG(WARNING) << "Exception thrown while exporting spans.";
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  bool ForceFlush(std::chrono::microseconds) noexcept override { return true; }
  bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

 private:
  opentelemetry::sdk::common::ExportResult ExportImpl(
      opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>> const&
          spans) {
    google::devtools::cloudtrace::v2::BatchWriteSpansRequest request;
    request.set_name(project_.FullName());
    for (auto& recordable : spans) {
      auto span = std::unique_ptr<otel_internal::Recordable>(
          static_cast<otel_internal::Recordable*>(recordable.release()));
      if (!span || !span->valid()) continue;
      *request.add_spans() = std::move(*span).as_proto();
    }

    auto status = client_.BatchWriteSpans(request);
    if (status.ok()) return opentelemetry::sdk::common::ExportResult::kSuccess;
    GCP_LOG(WARNING) << "Cloud Trace Export of " << request.spans().size()
                     << " span(s) failed with status=" << status;
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  Project project_;
  trace_v2::TraceServiceClient client_;
  otel_internal::Recordable::Generator generator_;
};

}  // namespace

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> MakeTraceExporter(
    Project project, Options options) {
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
