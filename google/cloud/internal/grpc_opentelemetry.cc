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

#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/options.h"
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace {

/**
 * A [carrier] for gRPC.
 *
 * This code originates from the following example:
 *
 * https://github.com/open-telemetry/opentelemetry-cpp/blob/d56a5c702fc2154ef82400df4801cd511ffb63ea/examples/grpc/tracer_common.h#L26-L45
 *
 * [carrier]:
 * https://opentelemetry.io/docs/reference/specification/context/api-propagators/#carrier
 */
class GrpcClientCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit GrpcClientCarrier(grpc::ClientContext& context)
      : context_(context) {}

  // Unneeded by clients.
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view) const noexcept override {
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    context_.AddMetadata({key.data(), key.size()},
                         {value.data(), value.size()});
  }

 private:
  grpc::ClientContext& context_;
};

}  // namespace

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanGrpc(
    opentelemetry::nostd::string_view service,
    opentelemetry::nostd::string_view method) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return GetTracer(internal::CurrentOptions())
      ->StartSpan(absl::StrCat(service.data(), "/", method.data()),
                  {{sc::kRpcSystem, sc::RpcSystemValues::kGrpc},
                   {sc::kRpcService, service},
                   {sc::kRpcMethod, method},
                   {sc::kNetTransport, sc::NetTransportValues::kIpTcp},
                   {"grpc.version", grpc::Version()}},
                  options);
}

void InjectTraceContext(grpc::ClientContext& context, Options const& options) {
  auto propagator = GetTextMapPropagator(options);
  auto current = opentelemetry::context::RuntimeContext::GetCurrent();
  GrpcClientCarrier carrier(context);
  propagator->Inject(carrier, current);
}

void ExtractAttributes(grpc::ClientContext& context,
                       opentelemetry::trace::Span& span) {
  // TODO(#10489): extract IP version, IP address, port from peer URI.
  // https://github.com/grpc/grpc/blob/master/src/core/lib/address_utils/parse_address.h
  span.SetAttribute("grpc.peer", context.peer());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
