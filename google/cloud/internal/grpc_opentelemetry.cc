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
std::map<std::string, std::string> MakeAttributes(
    std::pair<std::string, std::string> kv) {
  std::map<std::string, std::string> result;
  if (kv.first == ":grpc-context-peer") {
    // TODO(#10489): extract IP version, IP address, port from peer URI.
    // https://github.com/grpc/grpc/blob/master/src/core/lib/address_utils/parse_address.h
    // This is a hack-y solution until gRPC provides a way to parse the address:
    // https://github.com/grpc/grpc/issues/35885
    // The address should be in the format: host [ ":" port ]
    size_t offset = kv.second.find(':');
    if (offset == std::string::npos || offset == 0) {
      result.insert({/*sc::kServerAddress=*/"server.address", kv.second});
    } else {
      result.insert({/*sc::kServerAddress=*/"server.address",
                     kv.second.substr(0, offset)});
    }
    result.insert({"grpc.peer", std::move(kv.second)});
    return result;
  }
  if (kv.first == ":grpc-context-compression-algorithm") {
    result.insert({"grpc.compression_algorithm", std::move(kv.second)});
    return result;
  }
  if (!absl::EndsWith(kv.first, "-bin")) {
    result.insert({"rpc.grpc.response.metadata." + std::move(kv.first),
                   std::move(kv.second)});
    return result;
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
  result.insert(
      {"rpc.grpc.response.metadata." + std::move(kv.first), std::move(value)});
  return result;
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
                       opentelemetry::trace::Span& span) {
  auto md = GetRequestMetadataFromContext(context);
  for (auto& kv : md.headers) {
    for (auto const& pkv : MakeAttributes(std::move(kv))) {
      span.SetAttribute(pkv.first, pkv.second);
    }
  }
  for (auto& kv : md.trailers) {
    for (auto const& pkv : MakeAttributes(std::move(kv))) {
      span.SetAttribute(pkv.first, pkv.second);
    }
  }
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
