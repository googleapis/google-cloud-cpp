// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/data_tracing_connection.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/grpc_metrics_exporter.h"
#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/result_source_interface.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/internal/random.h"
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<bigtable::FailedMutation> MakeFailedMutations(Status const& status,
                                                          std::size_t n) {
  std::vector<bigtable::FailedMutation> mutations;
  mutations.reserve(n);
  for (int i = 0; i != static_cast<int>(n); ++i) {
    mutations.emplace_back(status, i);
  }
  return mutations;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::unique_ptr<bigtable_internal::OperationContextFactory>
MakeOperationContextFactory(
    Options const& options,
    std::vector<bigtable::InstanceResource> const& instances) {
  (void)instances;
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  if (options.get<bigtable::EnableMetricsOption>()) {
    std::shared_ptr<monitoring_v3::MetricServiceConnection> const
        metric_service_connection = monitoring_v3::MakeMetricServiceConnection(
            bigtable::internal::MetricsExporterConnectionOptions(options));
    auto gen = google::cloud::internal::MakeDefaultPRNG();
    std::string client_uid = google::cloud::internal::Sample(
        gen, 16, "abcdefghijklmnopqrstuvwxyz0123456789");
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
    if (bigtable::internal::IsDirectPath(options) && !instances.empty() &&
        options.get<bigtable::experimental::DirectPathMetricsModeOption>() ==
            bigtable::experimental::DirectPathMetricsMode::kEnabled) {
      bigtable_internal::EnableGrpcMetrics(metric_service_connection, options,
                                           client_uid);
    }
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
    return std::make_unique<bigtable_internal::MetricsOperationContextFactory>(
        std::move(client_uid), std::move(metric_service_connection), options);
  }
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  return std::make_unique<bigtable_internal::SimpleOperationContextFactory>();
}

struct DataConnectionComponents {
  Options options;
  std::unique_ptr<BackgroundThreads> background;
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth;
  std::shared_ptr<bigtable_internal::MutateRowsLimiter> limiter;
  std::unique_ptr<bigtable_internal::OperationContextFactory>
      operation_context_factory;
};

DataConnectionComponents MakeDataConnectionComponents(
    Options options, std::vector<bigtable::InstanceResource> const& instances,
    char const* location) {
  google::cloud::internal::CheckExpectedOptions<
      bigtable::AppProfileIdOption, CommonOptionList, GrpcOptionList,
      UnifiedCredentialsOptionList, bigtable::ClientOptionList,
      bigtable::DataPolicyOptionList>(options, location);
  if (!instances.empty()) {
    options.set<bigtable_internal::InstanceChannelAffinityOption>(instances);
  }
  options = bigtable::internal::DefaultDataOptions(std::move(options));
  std::unique_ptr<BackgroundThreads> background =
      google::cloud::internal::MakeBackgroundThreadsFactory(options)();
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth =
      google::cloud::internal::CreateAuthenticationStrategy(background->cq(),
                                                            options);
  std::shared_ptr<bigtable_internal::MutateRowsLimiter> limiter =
      bigtable_internal::MakeMutateRowsLimiter(background->cq(), options);
  std::unique_ptr<bigtable_internal::OperationContextFactory>
      operation_context_factory =
          MakeOperationContextFactory(options, instances);
  return {std::move(options), std::move(background), std::move(auth),
          std::move(limiter), std::move(operation_context_factory)};
}

std::shared_ptr<bigtable::DataConnection> MakeSingleStubDataConnection(
    DataConnectionComponents components) {
  std::shared_ptr<bigtable_internal::BigtableStub> stub =
      bigtable_internal::CreateBigtableStub(std::move(components.auth),
                                            components.background->cq(),
                                            components.options);

  return std::make_shared<bigtable_internal::DataConnectionImpl>(
      std::move(components.background),
      std::make_unique<bigtable_internal::StubManager>(std::move(stub)),
      std::move(components.operation_context_factory),
      std::move(components.limiter), std::move(components.options));
}

std::shared_ptr<bigtable::DataConnection> MakeInstanceAffinityDataConnection(
    DataConnectionComponents components,
    std::vector<bigtable::InstanceResource> const& instances) {
  auto stub_creation_fn = [auth = components.auth,
                           cq = components.background->cq(),
                           options = components.options](
                              std::string_view instance_name,
                              bigtable_internal::StubManager::Priming priming) {
    return bigtable_internal::CreateBigtableStub(auth, cq, instance_name,
                                                 priming, options);
  };

  absl::flat_hash_map<std::string,
                      std::shared_ptr<bigtable_internal::BigtableStub>>
      affinity_stubs = bigtable_internal::CreateBigtableAffinityStubs(
          instances, stub_creation_fn);
  return std::make_shared<bigtable_internal::DataConnectionImpl>(
      std::move(components.background),
      std::make_unique<bigtable_internal::StubManager>(
          std::move(affinity_stubs), stub_creation_fn),
      std::move(components.operation_context_factory),
      std::move(components.limiter), std::move(components.options));
}

std::shared_ptr<bigtable::DataConnection> MakeDirectPathDataConnection(
    DataConnectionComponents components,
    std::vector<bigtable::InstanceResource> const& instances) {
  Options directpath_options = components.options;
  directpath_options
      .set<::google::cloud::bigtable_internal::DataEndpointOption>(
          bigtable::internal::DefaultDirectPathDataEndpoint());
  directpath_options.set<EndpointOption>(
      bigtable::internal::DefaultDirectPathDataEndpoint());
  directpath_options.set<AuthorityOption>(
      bigtable::internal::DefaultDirectPathAuthority());
  directpath_options.set<bigtable::experimental::DirectPathModeOption>(
      bigtable::experimental::DirectPathMode::kEnabled);

  auto stub_creation_fn =
      [auth = components.auth, cq = components.background->cq(),
       directpath_options](std::string_view instance_name,
                           bigtable_internal::StubManager::Priming priming) {
        return bigtable_internal::CreateBigtableStub(
            auth, cq, instance_name, priming, directpath_options);
      };
  absl::flat_hash_map<std::string,
                      std::shared_ptr<bigtable_internal::BigtableStub>>
      affinity_stubs = bigtable_internal::CreateBigtableAffinityStubs(
          instances, stub_creation_fn);
  std::unique_ptr<bigtable_internal::StubManager> stub_manager =
      std::make_unique<bigtable_internal::StubManager>(
          std::move(affinity_stubs), stub_creation_fn);

  return std::make_shared<bigtable_internal::DataConnectionImpl>(
      std::move(components.background), std::move(stub_manager),
      std::move(components.operation_context_factory),
      std::move(components.limiter), std::move(directpath_options));
}

std::shared_ptr<bigtable::DataConnection> WrapDataTracing(
    std::shared_ptr<bigtable::DataConnection> conn) {
  if (google::cloud::internal::TracingEnabled(conn->options())) {
    return bigtable_internal::MakeDataTracingConnection(std::move(conn));
  }
  return conn;
}

}  // namespace

DataConnection::~DataConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status DataConnection::Apply(std::string const&, SingleRowMutation) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<Status> DataConnection::AsyncApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, SingleRowMutation) {
  return make_ready_future(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

std::vector<FailedMutation> DataConnection::BulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, BulkMutation mut) {
  return bigtable_internal::MakeFailedMutations(
      Status(StatusCode::kUnimplemented, "not-implemented"), mut.size());
}

future<std::vector<FailedMutation>> DataConnection::AsyncBulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, BulkMutation mut) {
  return make_ready_future(bigtable_internal::MakeFailedMutations(
      Status(StatusCode::kUnimplemented, "not-implemented"), mut.size()));
}

RowReader DataConnection::ReadRows(std::string const& table_name,
                                   RowSet row_set, std::int64_t rows_limit,
                                   Filter filter) {
  auto const& options = google::cloud::internal::CurrentOptions();
  return ReadRowsFull(ReadRowsParams{
      std::move(table_name),
      options.get<AppProfileIdOption>(),
      std::move(row_set),
      rows_limit,
      std::move(filter),
      options.get<ReverseScanOption>(),
  });
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
RowReader DataConnection::ReadRowsFull(ReadRowsParams) {
  return MakeRowReader(std::make_shared<bigtable_internal::StatusOnlyRowReader>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

StatusOr<std::pair<bool, Row>> DataConnection::ReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string, Filter) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<MutationBranch> DataConnection::CheckAndMutateRow(
    std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<MutationBranch>> DataConnection::AsyncCheckAndMutateRow(
    std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return make_ready_future<StatusOr<MutationBranch>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<std::vector<RowKeySample>> DataConnection::SampleRows(
    std::string const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<std::vector<RowKeySample>>> DataConnection::AsyncSampleRows(
    std::string const&) {
  return make_ready_future<StatusOr<std::vector<RowKeySample>>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<Row> DataConnection::ReadModifyWriteRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::bigtable::v2::ReadModifyWriteRowRequest) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<Row>> DataConnection::AsyncReadModifyWriteRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::bigtable::v2::ReadModifyWriteRowRequest) {
  return make_ready_future<StatusOr<Row>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

void DataConnection::AsyncReadRows(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::function<future<bool>(Row)>,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::function<void(Status)> on_finish, RowSet, std::int64_t, Filter) {
  on_finish(Status(StatusCode::kUnimplemented, "not implemented"));
}

future<StatusOr<std::pair<bool, Row>>> DataConnection::AsyncReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string, Filter) {
  return make_ready_future<StatusOr<std::pair<bool, Row>>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<bigtable::PreparedQuery> DataConnection::PrepareQuery(
    bigtable::PrepareQueryParams const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}
future<StatusOr<bigtable::PreparedQuery>> DataConnection::AsyncPrepareQuery(
    bigtable::PrepareQueryParams const&) {
  return make_ready_future<StatusOr<PreparedQuery>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

bigtable::RowStream DataConnection::ExecuteQuery(bigtable::ExecuteQueryParams) {
  return RowStream(
      std::make_unique<bigtable_internal::StatusOnlyResultSetSource>(
          Status(StatusCode::kUnimplemented, "not implemented")));
}

std::shared_ptr<DataConnection> MakeDataConnection(
    std::vector<InstanceResource> const& instances, Options options) {
  DataConnectionComponents components =
      MakeDataConnectionComponents(std::move(options), instances, __func__);

  std::shared_ptr<DataConnection> conn;
  if (!instances.empty() &&
      bigtable::internal::IsDirectPath(components.options)) {
    conn = MakeDirectPathDataConnection(std::move(components), instances);
  } else {
    conn = MakeInstanceAffinityDataConnection(std::move(components), instances);
  }
  return WrapDataTracing(std::move(conn));
}

std::shared_ptr<DataConnection> MakeDataConnection(Options options) {
  DataConnectionComponents components =
      MakeDataConnectionComponents(std::move(options), {}, __func__);
  std::shared_ptr<DataConnection> conn =
      MakeSingleStubDataConnection(std::move(components));
  return WrapDataTracing(std::move(conn));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
