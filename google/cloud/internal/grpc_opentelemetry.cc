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
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/options.h"
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

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

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
