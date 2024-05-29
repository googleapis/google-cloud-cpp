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

#include "google/cloud/spanner/internal/spanner_stub_factory.h"
#include "google/cloud/spanner/internal/spanner_auth_decorator.h"
#include "google/cloud/spanner/internal/spanner_logging_decorator.h"
#include "google/cloud/spanner/internal/spanner_metadata_decorator.h"
#include "google/cloud/spanner/internal/spanner_tracing_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<SpannerStub> DecorateSpannerStub(
    std::shared_ptr<SpannerStub> stub, spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts) {
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<SpannerAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<SpannerMetadata>(
      std::move(stub),
      std::multimap<std::string, std::string>{
          {"google-cloud-resource-prefix", db.FullName()}},
      internal::HandCraftedLibClientHeader());
  if (internal::Contains(opts.get<LoggingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<SpannerLogging>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>(),
        opts.get<LoggingComponentsOption>());
  }
  if (internal::TracingEnabled(opts)) {
    stub = MakeSpannerTracingStub(std::move(stub));
  }
  return stub;
}

std::shared_ptr<SpannerStub> CreateDefaultSpannerStub(
    spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts, int channel_id) {
  grpc::ChannelArguments channel_arguments =
      internal::MakeChannelArguments(opts);
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);

  auto channel =
      auth->CreateChannel(opts.get<EndpointOption>(), channel_arguments);
  auto spanner_grpc_stub = google::spanner::v1::Spanner::NewStub(channel);
  auto stub =
      std::make_shared<DefaultSpannerStub>(std::move(spanner_grpc_stub));
  return DecorateSpannerStub(std::move(stub), db, std::move(auth), opts);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
