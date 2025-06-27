// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/internal/bigtable_auth_decorator.h"
#include "google/cloud/bigtable/internal/bigtable_channel_refresh.h"
#include "google/cloud/bigtable/internal/bigtable_logging_decorator.h"
#include "google/cloud/bigtable/internal/bigtable_metadata_decorator.h"
#include "google/cloud/bigtable/internal/bigtable_round_robin_decorator.h"
#include "google/cloud/bigtable/internal/bigtable_tracing_stub.h"
#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include "google/bigtable/v2/feature_flags.pb.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    internal::GrpcAuthenticationStrategy& auth, Options const& options,
    int channel_id) {
  auto args = internal::MakeChannelArguments(options);
  args.SetInt("grpc.channel_id", channel_id);
  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

std::string FeaturesMetadata() {
  static auto const* const kFeatures = new auto([] {
    google::bigtable::v2::FeatureFlags proto;
    proto.set_reverse_scans(true);
    proto.set_last_scanned_row_responses(true);
    proto.set_mutate_rows_rate_limit(true);
    proto.set_mutate_rows_rate_limit2(true);
    proto.set_routing_cookie(true);
    proto.set_retry_info(true);
    return internal::UrlsafeBase64Encode(proto.SerializeAsString());
  }());
  return *kFeatures;
}

}  // namespace

std::shared_ptr<BigtableStub> CreateBigtableStubRoundRobin(
    Options const& options,
    std::function<std::shared_ptr<BigtableStub>(int)> child_factory) {
  std::vector<std::shared_ptr<BigtableStub>> children(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(children.begin(), children.end(),
                [&id, &child_factory] { return child_factory(id++); });
  return std::make_shared<BigtableRoundRobin>(std::move(children));
}

std::shared_ptr<BigtableStub> CreateDecoratedStubs(
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    CompletionQueue const& cq, Options const& options,
    BaseBigtableStubFactory const& base_factory) {
  auto cq_impl = internal::GetCompletionQueueImpl(cq);
  auto refresh = std::make_shared<ConnectionRefreshState>(
      cq_impl, options.get<bigtable::MinConnectionRefreshOption>(),
      options.get<bigtable::MaxConnectionRefreshOption>());
  auto child_factory = [base_factory, cq_impl, refresh, &auth,
                        options](int id) {
    auto channel = CreateGrpcChannel(*auth, options, id);
    if (refresh->enabled()) ScheduleChannelRefresh(cq_impl, refresh, channel);
    return base_factory(std::move(channel));
  };
  auto stub = CreateBigtableStubRoundRobin(options, std::move(child_factory));
  if (refresh->enabled()) {
    stub = std::make_shared<BigtableChannelRefresh>(std::move(stub),
                                                    std::move(refresh));
  }
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<BigtableAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<BigtableMetadata>(
      std::move(stub),
      std::multimap<std::string, std::string>{
          {"bigtable-features", FeaturesMetadata()}},
      internal::HandCraftedLibClientHeader());
  if (internal::Contains(options.get<LoggingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<BigtableLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<LoggingComponentsOption>());
  }
  if (internal::TracingEnabled(options)) {
    stub = MakeBigtableTracingStub(std::move(stub));
  }
  return stub;
}

std::shared_ptr<BigtableStub> CreateBigtableStub(
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    CompletionQueue const& cq, Options const& options) {
  return CreateDecoratedStubs(
      std::move(auth), cq, options, [](std::shared_ptr<grpc::Channel> c) {
        return std::make_shared<DefaultBigtableStub>(
            google::bigtable::v2::Bigtable::NewStub(std::move(c)));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
