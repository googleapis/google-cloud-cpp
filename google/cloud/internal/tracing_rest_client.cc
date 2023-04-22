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

#include "google/cloud/internal/tracing_rest_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/rest_opentelemetry.h"
#include "google/cloud/internal/tracing_http_payload.h"
#include "google/cloud/internal/tracing_rest_response.h"
#include "absl/strings/numbers.h"
#include <array>
#include <cstdint>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

// We always inject the CloudTraceContext. Typically, these requests are going
// to GCP where the X-Cloud-Trace-Context header can connect the request with
// the internal tracing service.
void InjectCloudTraceContext(RestContext& ctx,
                             opentelemetry::trace::Span const& span) {
  auto context = span.GetContext();
  if (!context.IsValid()) return;

  using opentelemetry::trace::SpanId;
  using opentelemetry::trace::TraceId;
  std::array<char, 2 * TraceId::kSize> trace_id;
  context.trace_id().ToLowerBase16(trace_id);
  std::array<char, 2 * SpanId::kSize> span_id;
  context.span_id().ToLowerBase16(span_id);
  std::uint64_t value;
  if (!absl::SimpleHexAtoi(absl::string_view{span_id.data(), span_id.size()},
                           &value)) {
    return;
  }
  ctx.AddHeader(
      "X-Cloud-Trace-Context",
      absl::StrCat(absl::string_view{trace_id.data(), trace_id.size()}, "/",
                   value, ";o=1"));
}

}  // namespace

void ExtractAttributes(StatusOr<std::unique_ptr<RestResponse>> const& value,
                       opentelemetry::trace::Span& span) {
  if (!value) return;
  auto const& response = *value;
  if (!response) return;
  for (auto const& kv : response->Headers()) {
    span.SetAttribute("http.response.header." + kv.first, kv.second);
  }
}

/**
 * Extracts information from @p value, and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 *
 * Note that this function should be called after the RPC has finished.
 */
StatusOr<std::unique_ptr<RestResponse>> EndResponseSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    StatusOr<std::unique_ptr<RestResponse>> value) {
  if (!value) return internal::EndSpan(*span, std::move(value));
  ExtractAttributes(value, *span);
  return std::unique_ptr<RestResponse>(std::make_unique<TracingRestResponse>(
      *std::move(value), std::move(span)));
}

TracingRestClient::TracingRestClient(std::unique_ptr<RestClient> impl)
    : impl_(std::move(impl)) {}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Delete(
    RestContext& context, RestRequest const& request) {
  auto span = MakeSpanHttp(request, "DELETE");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), impl_->Delete(context, request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Get(
    RestContext& context, RestRequest const& request) {
  auto span = MakeSpanHttp(request, "GET");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), impl_->Get(context, request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Patch(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "PATCH");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span),
                         impl_->Patch(context, request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "POST");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span),
                         impl_->Post(context, request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  auto span = MakeSpanHttp(request, "PUT");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span),
                         impl_->Post(context, request, form_data));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Put(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "PUT");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span),
                         impl_->Put(context, request, payload));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::unique_ptr<RestClient> MakeTracingRestClient(
    std::unique_ptr<RestClient> client) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return std::make_unique<TracingRestClient>(std::move(client));
#else
  return client;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
