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

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

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
void InjectTraceContext(grpc::ClientContext& context, Options const& options);

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_OPENTELEMETRY_H
