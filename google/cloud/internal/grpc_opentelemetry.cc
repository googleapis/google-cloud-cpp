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
#include "google/cloud/internal/grpc_metadata_view.h"
#include "google/cloud/internal/grpc_request_metadata.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "absl/strings/match.h"
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
    GCP_LOG(FATAL) << __func__ << " should never be called";
    return opentelemetry::nostd::string_view();
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    internal::NoExceptAction([this, key, value] {
      context_.AddMetadata({key.data(), key.size()},
                           {value.data(), value.size()});
    });
  }

 private:
  grpc::ClientContext& context_;
};

// We translate some keys, and modify binary values to be printable.
std::pair<std::string, std::string> MakeAttribute(
    std::pair<std::string, std::string> kv) {
  if (kv.first == ":grpc-context-peer") {
    // TODO(#10489): extract IP version, IP address, port from peer URI.
    // https://github.com/grpc/grpc/blob/master/src/core/lib/address_utils/parse_address.h
    return {"grpc.peer", std::move(kv.second)};
  }
  if (kv.first == ":grpc-context-compression-algorithm") {
    return {"grpc.compression_algorithm", std::move(kv.second)};
  }
  if (!absl::EndsWith(kv.first, "-bin")) {
    return {"rpc.grpc.response.metadata." + std::move(kv.first),
            std::move(kv.second)};
  }

  // The header is in binary format. OpenTelemetry does not really support byte
  // arrays in their attributes, so let's transform the value into a string that
  // can be printed and interpreted.
  std::string value;
  auto constexpr kDigits = "0123456789ABCDEF";
  for (unsigned char c : kv.second) {
    value.push_back('\\');
    value.push_back('x');
    value.push_back(kDigits[(c >> 4) & 0xf]);
    value.push_back(kDigits[c & 0xf]);
  }
  return {"rpc.grpc.response.metadata." + std::move(kv.first),
          std::move(value)};
}

}  // namespace

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanGrpc(
    opentelemetry::nostd::string_view service,
    opentelemetry::nostd::string_view method) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return internal::MakeSpan(
      absl::StrCat(absl::string_view{service.data(), service.size()}, "/",
                   absl::string_view{method.data(), method.size()}),
      {{sc::kRpcSystem, sc::RpcSystemValues::kGrpc},
       {sc::kRpcService, service},
       {sc::kRpcMethod, method},
       {/*sc::kNetworkTransport=*/"network.transport",
        sc::NetTransportValues::kIpTcp},
       {"grpc.version", grpc::Version()}},
      options);
}

void InjectTraceContext(
    grpc::ClientContext& context,
    opentelemetry::context::propagation::TextMapPropagator& propagator) {
  auto current = opentelemetry::context::RuntimeContext::GetCurrent();
  GrpcClientCarrier carrier(context);
  propagator.Inject(carrier, current);
}

void ExtractAttributes(grpc::ClientContext& context,
                       opentelemetry::trace::Span& span,
                       GrpcMetadataView view) {
  auto md = GetRequestMetadataFromContext(context, view);
  for (auto& kv : md.headers) {
    auto p = MakeAttribute(std::move(kv));
    span.SetAttribute(p.first, p.second);
  }
  for (auto& kv : md.trailers) {
    auto p = MakeAttribute(std::move(kv));
    span.SetAttribute(p.first, p.second);
  }
}

future<Status> EndSpan(
    std::shared_ptr<grpc::ClientContext> context,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    future<Status> fut) {
  return fut.then([oc = opentelemetry::context::RuntimeContext::GetCurrent(),
                   c = std::move(context), s = std::move(span)](auto f) {
    auto t = f.get();
    // If the error is client originated, then do not make the call to get the
    // gRPC server metadata. Since the call to GetInitialServerMetadata()
    // *might* crash the program.
    ExtractAttributes(*c, *s,
                      IsClientOrigin(t)
                          ? GrpcMetadataView::kWithoutServerMetadata
                          : GrpcMetadataView::kWithServerMetadata);
    DetachOTelContext(oc);
    return EndSpan(*s, std::move(t));
  });
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
