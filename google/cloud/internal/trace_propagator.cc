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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "absl/strings/numbers.h"
#include <opentelemetry/context/propagation/composite_propagator.h>
#include <opentelemetry/nostd/span.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <cstdlib>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

/**
 * A context propagator, specifically for Google Cloud.
 *
 * @see https://cloud.google.com/trace/docs/setup#force-trace for the
 * implementation specification.
 */
class CloudTraceContext
    : public opentelemetry::context::propagation::TextMapPropagator {
 public:
  explicit CloudTraceContext() = default;

  // Returns the context that is stored in the carrier with the TextMapCarrier
  // as extractor.
  opentelemetry::context::Context Extract(
      opentelemetry::context::propagation::TextMapCarrier const&,
      opentelemetry::context::Context& context) noexcept override {
    // Client libraries do not need to extract trace context. We only ever
    // initiate outgoing requests. We do not receive incoming requests.
    return context;
  }

  // Sets the context for carrier with self defined rules.
  void Inject(
      opentelemetry::context::propagation::TextMapCarrier& carrier,
      opentelemetry::context::Context const& context) noexcept override {
    auto span_context = opentelemetry::trace::GetSpan(context)->GetContext();
    if (!span_context.IsValid()) return;

    // Format: X-Cloud-Trace-Context: TRACE_ID/SPAN_ID;o=TRACE_TRUE
    // Where (annoyingly) SPAN_ID is in decimal, not hex.
    using opentelemetry::trace::SpanId;
    using opentelemetry::trace::TraceId;
    std::array<char, 2 * TraceId::kSize> trace_id;
    span_context.trace_id().ToLowerBase16(trace_id);
    // We would prefer to use `absl::SimpleHexAtoi`, but it is not available in
    // the oldest version of Abseil we support. So we use `std::strtoull`, which
    // requires the input to be null-terminated.
    std::array<char, 2 * SpanId::kSize + 1> span_id;
    span_context.span_id().ToLowerBase16(
        opentelemetry::nostd::span<char, 2 * SpanId::kSize>{
            span_id.data(), span_id.size() - 1});
    span_id[2 * SpanId::kSize] = '\0';
    char* end = nullptr;
    std::uint64_t span_id_dec = std::strtoull(span_id.data(), &end, 16);
    if (end - span_id.data() != 2 * SpanId::kSize) return;
    carrier.Set(
        "x-cloud-trace-context",
        absl::StrCat(absl::string_view{trace_id.data(), trace_id.size()}, "/",
                     span_id_dec, ";o=", span_context.IsSampled() ? "1" : "0"));
  }

  // Gets the fields set in the carrier by the `inject` method
  bool Fields(opentelemetry::nostd::function_ref<
              bool(opentelemetry::nostd::string_view)>
                  callback) const noexcept override {
    return callback("x-cloud-trace-context");
  }
};
}  // namespace

std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>
MakePropagator() {
  std::vector<
      std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>>
      v;
  v.push_back(std::make_unique<CloudTraceContext>());
  v.push_back(
      std::make_unique<opentelemetry::trace::propagation::HttpTraceContext>());
  return std::make_unique<
      opentelemetry::context::propagation::CompositePropagator>(std::move(v));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
