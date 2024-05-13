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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPENTELEMETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPENTELEMETRY_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/grpc_metadata_view.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/status_utils.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <chrono>
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

/**
 * Make a span, setting attributes related to gRPC.
 *
 * @see
 * https://opentelemetry.io/docs/reference/specification/trace/semantic_conventions/rpc/
 * for the semantic conventions used for span names and attributes.
 */
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanGrpc(
    opentelemetry::nostd::string_view service,
    opentelemetry::nostd::string_view method);

/**
 * Propagate trace context for an outbound gRPC call.
 *
 * The trace context is added as metadata in the `grpc::ClientContext`. By
 * injecting the trace context, we can potentially pick up a client side span
 * from within Google's servers.
 *
 * The format of the metadata is determined by the `TextMapPropagator` used for
 * the given call. Circa 2023-01, Google expects an `X-Cloud-Trace-Context`
 * [header].
 *
 * @see https://opentelemetry.io/docs/concepts/instrumenting-library/#injecting-context
 *
 * [header]: https://cloud.google.com/trace/docs/setup#force-trace
 */
void InjectTraceContext(
    grpc::ClientContext& context,
    opentelemetry::context::propagation::TextMapPropagator& propagator);

/**
 * Extracts attributes from the `context` and adds them to the `span`.
 */
void ExtractAttributes(grpc::ClientContext& context,
                       opentelemetry::trace::Span& span, GrpcMetadataView view);

/**
 * Extracts information from the `grpc::ClientContext`, and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 *
 * Note that this function should be called after the RPC has finished.
 */
template <typename T>
T EndSpan(grpc::ClientContext& context, opentelemetry::trace::Span& span,
          T value) {
  ExtractAttributes(context, span, GrpcMetadataView::kWithServerMetadata);
  return EndSpan(span, std::move(value));
}

future<Status> EndSpan(
    std::shared_ptr<grpc::ClientContext> context,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    future<Status> fut);

template <typename T>
future<StatusOr<T>> EndSpan(
    std::shared_ptr<grpc::ClientContext> context,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    future<StatusOr<T>> fut) {
  return fut.then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
                   c = std::move(context), s = std::move(span)](auto f) {
    auto t = f.get();
    // If the error is client originated, then do not make the call to get the
    // gRPC server metadata. Since the call to GetInitialServerMetadata()
    // *might* crash the program.
    ExtractAttributes(*c, *s,
                      IsClientOrigin(t.status())
                          ? GrpcMetadataView::kWithoutServerMetadata
                          : GrpcMetadataView::kWithServerMetadata);
    DetachOTelContext(oc);
    return EndSpan(*s, std::move(t));
  });
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

/**
 * Returns a traced timer, if OpenTelemetry tracing is enabled.
 */
template <typename Rep, typename Period>
future<StatusOr<std::chrono::system_clock::time_point>> TracedAsyncBackoff(
    CompletionQueue& cq, Options const& options,
    std::chrono::duration<Rep, Period> duration, std::string const& name) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (TracingEnabled(options)) {
    auto span = MakeSpan(name);
    OTelScope scope(span);
    auto timer = cq.MakeRelativeTimer(duration);
    return EndSpan(std::move(span), std::move(timer));
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  (void)options;
  (void)name;
  return cq.MakeRelativeTimer(duration);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPENTELEMETRY_H
