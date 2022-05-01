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
#include "google/cloud/bigtable/internal/bigtable_round_robin.h"
#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    std::shared_ptr<CompletionQueue> const& cq,
    std::shared_ptr<ConnectionRefreshState> const& refresh_state,
    internal::GrpcAuthenticationStrategy& auth, Options const& options,
    int channel_id) {
  auto args = internal::MakeChannelArguments(options);
  args.SetInt("grpc.channel_id", channel_id);
  auto c = auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
  if (refresh_state->enabled()) {
    ScheduleChannelRefresh(cq, refresh_state, c);
  }
  return c;
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
    google::cloud::CompletionQueue cq, Options const& options,
    BaseBigtableStubFactory const& base_factory) {
  auto cq_ptr = std::make_shared<CompletionQueue>(cq);
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      cq_ptr, options.get<bigtable::MinConnectionRefreshOption>(),
      options.get<bigtable::MaxConnectionRefreshOption>());
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto child_factory = [base_factory, cq_ptr, refresh_state, &auth,
                        options](int id) {
    auto channel = CreateGrpcChannel(cq_ptr, refresh_state, *auth, options, id);
    return base_factory(std::move(channel));
  };
  auto stub = CreateBigtableStubRoundRobin(options, std::move(child_factory));
  if (refresh_state->enabled()) {
    stub = std::make_shared<BigtableChannelRefresh>(std::move(stub),
                                                    std::move(refresh_state));
  }
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<BigtableAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<BigtableMetadata>(std::move(stub));
  if (google::cloud::internal::Contains(options.get<TracingComponentsOption>(),
                                        "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<BigtableLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<TracingComponentsOption>());
  }
  return stub;
}

std::shared_ptr<BigtableStub> CreateBigtableStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  return CreateDecoratedStubs(
      std::move(cq), options, [](std::shared_ptr<grpc::Channel> c) {
        return std::make_shared<DefaultBigtableStub>(
            google::bigtable::v2::Bigtable::NewStub(std::move(c)));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
