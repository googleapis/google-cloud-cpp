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
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/rest_opentelemetry.h"
#include "google/cloud/internal/tracing_http_payload.h"
#include "google/cloud/internal/tracing_rest_response.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

void ExtractAttributes(StatusOr<std::unique_ptr<RestResponse>> const& value,
                       opentelemetry::trace::Span& span) {
  if (!value) return;
  auto const& response = *value;
  if (!response) return;
  for (auto const& kv : response->Headers()) {
    span.SetAttribute("http.header." + kv.first, kv.second);
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
StatusOr<std::unique_ptr<RestResponse>> EndSpan(
    opentelemetry::trace::Span& request_span,
    StatusOr<std::unique_ptr<RestResponse>> value) {
  if (!value) return internal::EndSpan(request_span, std::move(value));
  auto payload_span = MakeSpanHttpPayload(request_span);
  ExtractAttributes(value, *payload_span);
  value = std::make_unique<TracingRestResponse>(*std::move(value),
                                                std::move(payload_span));
  return internal::EndSpan(request_span, std::move(value));
}

TracingRestClient::TracingRestClient(std::unique_ptr<RestClient> impl)
    : impl_(std::move(impl)) {}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Delete(
    RestRequest const& request) {
  auto span = MakeSpanHttp(request, "DELETE");
  return EndSpan(*span, impl_->Delete(request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Get(
    RestRequest const& request) {
  auto span = MakeSpanHttp(request, "GET");
  return EndSpan(*span, impl_->Get(request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Patch(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "POST");
  return EndSpan(*span, impl_->Patch(request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "POST");
  return EndSpan(*span, impl_->Post(request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestRequest request,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  auto span = MakeSpanHttp(request, "PUT");
  return EndSpan(*span, impl_->Post(request, form_data));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Put(
    RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "PUT");
  return EndSpan(*span, impl_->Put(request, payload));
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
