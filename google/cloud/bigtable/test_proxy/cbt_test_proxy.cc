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

#include "google/cloud/bigtable/test_proxy/cbt_test_proxy.h"
#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include "absl/time/time.h"
#include <google/protobuf/util/time_util.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
namespace test_proxy {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
namespace v2 = ::google::bigtable::v2;
namespace testpb = ::google::bigtable::testproxy;

// Convert `Status` to `grpc::Status`, discarding any `details`.
::grpc::Status ToGrpcStatus(Status const& status) {
  return ::grpc::Status(static_cast<grpc::StatusCode>(status.code()),
                        std::move(status.message()));
}

// Convert `Status` to `rpc::Status`, discarding any `details`.
google::rpc::Status ToRpcStatus(Status const& status) {
  google::rpc::Status ret;
  ret.set_code(static_cast<int>(status.code()));
  ret.set_message(status.message().data(), status.message().size());
  return ret;
}

// Helper method to convert cbt row to v2 new_row;
v2::Row ConvertRowToV2(Row const& row) {
  v2::Row new_row;
  new_row.set_key(row.row_key());

  // The results from the open source library are presented in a different style
  // than the results from the V2 API. In order not to hide unexpected client
  // behavior, we do not reorder the results. This means that the same family
  // name or column qualifier may appear multiple times.
  //
  // We do, however, combine consecutive cells with the same family or column
  // qualifier where possible.
  v2::Family* family = nullptr;
  v2::Column* column = nullptr;

  for (Cell const& cell : row.cells()) {
    if (family == nullptr || family->name() != cell.family_name()) {
      family = new_row.add_families();
      family->set_name(cell.family_name());
      column = nullptr;
    }
    if (column == nullptr || column->qualifier() != cell.column_qualifier()) {
      column = family->add_columns();
      column->set_qualifier(cell.column_qualifier());
    }
    v2::Cell* new_cell = column->add_cells();
    new_cell->set_timestamp_micros(cell.timestamp().count());
    new_cell->set_value(std::move(cell.value()));
  }
  return new_row;
}

}  // namespace

grpc::Status CbtTestProxy::CreateClient(
    ::grpc::ServerContext*, testpb::CreateClientRequest const* request,
    testpb::CreateClientResponse*) {
  if (request->client_id().empty() || request->data_target().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "both `client_id` and `data_target` must be provided.");
  }

  auto options =
      Options{}
          .set<EndpointOption>(request->data_target())
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<MaxConnectionRefreshOption>(std::chrono::milliseconds(0))
          .set<AppProfileIdOption>(request->app_profile_id());

  if (request->has_per_operation_timeout()) {
    auto duration = absl::ToChronoMilliseconds(absl::Milliseconds(
        google::protobuf::util::TimeUtil::DurationToMilliseconds(
            request->per_operation_timeout())));
    options
        .set<DataRetryPolicyOption>(
            DataLimitedTimeRetryPolicy(duration).clone())
        // TODO(#4926) - set deadlines using a nicer API.
        .set<google::cloud::internal::GrpcSetupOption>(
            [duration](grpc::ClientContext& context) {
              auto deadline = std::chrono::system_clock::now() + duration;
              if (context.deadline() >= deadline) {
                context.set_deadline(deadline);
              }
            });
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto client = connections_.find(request->client_id());
  if (client != connections_.end()) {
    return grpc::Status(
        grpc::StatusCode::ALREADY_EXISTS,
        absl::StrCat("Client ", request->client_id(), " already exists."));
  }
  connections_.insert({request->client_id(), MakeDataConnection(options)});
  return grpc::Status();
}

// C++ client doesn't support closing which will make the client not accept new
// calls for operation. As such, CloseClient() is implemented as no-op.
grpc::Status CbtTestProxy::CloseClient(::grpc::ServerContext*,
                                       testpb::CloseClientRequest const*,
                                       testpb::CloseClientResponse*) {
  return grpc::Status();
}

grpc::Status CbtTestProxy::RemoveClient(
    ::grpc::ServerContext*, testpb::RemoveClientRequest const* request,
    testpb::RemoveClientResponse*) {
  std::lock_guard<std::mutex> lk(mu_);
  auto client = connections_.find(request->client_id());
  if (client == connections_.end()) {
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        absl::StrCat("Client ", request->client_id(), " not found."));
  }

  connections_.erase(request->client_id());
  return grpc::Status();
}

grpc::Status CbtTestProxy::ReadRow(::grpc::ServerContext*,
                                   testpb::ReadRowRequest const* request,
                                   testpb::RowResult* response) {
  auto table = GetTableFromRequest(request->client_id(), request->table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  auto const row =
      table->ReadRow(request->row_key(), Filter(request->filter()));
  if (row.ok()) {
    if (row->first) {
      *response->mutable_row() = ConvertRowToV2(row->second);
    } else {
      GCP_LOG(INFO) << "Received empty row: " << request->row_key();
    }
  }
  *response->mutable_status() = ToRpcStatus(row.status());
  return grpc::Status();
}

grpc::Status CbtTestProxy::ReadRows(::grpc::ServerContext*,
                                    testpb::ReadRowsRequest const* request,
                                    testpb::RowsResult* response) {
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  RowSet row_set;
  for (RowKeyType const& row_key : request->request().rows().row_keys()) {
    row_set.Append(std::string(row_key));
  }
  for (v2::RowRange const& row_range : request->request().rows().row_ranges()) {
    row_set.Append(RowRange(row_range));
  }
  Filter const filter(request->request().filter());
  RowReader reader = table->ReadRows(
      std::move(row_set),
      std::max(static_cast<int64_t>(0), request->request().rows_limit()),
      filter);

  Status status;
  for (auto const& row : reader) {
    if (row.ok()) {
      *response->add_rows() = ConvertRowToV2(*std::move(row));
    } else {
      status = row.status();
      GCP_LOG(INFO) << "Error reading row: " << row.status();
    }

    if (request->cancel_after_rows() > 0 &&
        response->rows_size() >= request->cancel_after_rows()) {
      reader.Cancel();
      GCP_LOG(INFO) << "Canceling ReadRows() to respect cancel_after_rows="
                    << request->cancel_after_rows();
      break;
    }
  }

  *response->mutable_status() = ToRpcStatus(status);
  return grpc::Status();
}

grpc::Status CbtTestProxy::MutateRow(::grpc::ServerContext*,
                                     testpb::MutateRowRequest const* request,
                                     testpb::MutateRowResult* response) {
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  SingleRowMutation mutation(request->request());
  *response->mutable_status() = ToRpcStatus(table->Apply(std::move(mutation)));
  return grpc::Status();
}

grpc::Status CbtTestProxy::BulkMutateRows(
    ::grpc::ServerContext*, testpb::MutateRowsRequest const* request,
    testpb::MutateRowsResult* response) {
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  BulkMutation mutation;
  for (auto const& entry : request->request().entries()) {
    mutation.push_back(SingleRowMutation(entry));
  }

  auto failed = table->BulkApply(std::move(mutation));
  for (auto const& failure : failed) {
    auto& entry = *response->add_entries();
    entry.set_index(failure.original_index());
    *entry.mutable_status() = ToRpcStatus(failure.status());
  }
  return grpc::Status();
}

grpc::Status CbtTestProxy::CheckAndMutateRow(
    ::grpc::ServerContext*, testpb::CheckAndMutateRowRequest const* request,
    testpb::CheckAndMutateRowResult* response) {
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  std::string const& row_key = std::string(request->request().row_key());
  Filter filter(request->request().predicate_filter());
  std::vector<Mutation> true_mutations;
  std::vector<Mutation> false_mutations;
  for (auto const& mutation_v2 : request->request().true_mutations()) {
    Mutation mutation;
    mutation.op = mutation_v2;
    true_mutations.emplace_back(mutation);
  }
  for (auto const& mutation_v2 : request->request().false_mutations()) {
    Mutation mutation;
    mutation.op = mutation_v2;
    false_mutations.emplace_back(mutation);
  }
  auto branch = table->CheckAndMutateRow(row_key, std::move(filter),
                                         std::move(true_mutations),
                                         std::move(false_mutations));

  *response->mutable_status() = ToRpcStatus(branch.status());
  if (!branch) return grpc::Status();

  response->mutable_result()->set_predicate_matched(
      *branch == MutationBranch::kPredicateMatched);
  return grpc::Status();
}

grpc::Status CbtTestProxy::SampleRowKeys(
    ::grpc::ServerContext*, testpb::SampleRowKeysRequest const* request,
    testpb::SampleRowKeysResult* response) {
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  auto samples = table->SampleRows();
  *response->mutable_status() = ToRpcStatus(samples.status());
  if (!samples.ok()) return grpc::Status();

  for (auto const& sample : *samples) {
    auto& sample_key = *response->add_samples();
    sample_key.set_row_key(sample.row_key);
    sample_key.set_offset_bytes(sample.offset_bytes);
  }
  return grpc::Status();
}

grpc::Status CbtTestProxy::ReadModifyWriteRow(
    ::grpc::ServerContext*, testpb::ReadModifyWriteRowRequest const* request,
    testpb::RowResult* response) {
  if (request->request().rules_size() > 5) {
    GCP_LOG(ERROR) << "Failed to ReadModifyWriteRow.";
    return grpc::Status(
        grpc::StatusCode::UNIMPLEMENTED,
        "Incoming request has more than 5 modify rules. Not implemented.");
  }
  if (request->request().rules().empty()) {
    GCP_LOG(ERROR) << "Failed to ReadModifyWriteRow.";
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Incoming request has no rules. Not supported.");
  }
  auto table = GetTableFromRequest(request->client_id(),
                                   request->request().table_name());
  if (!table.ok()) return ToGrpcStatus(std::move(table).status());

  std::vector<ReadModifyWriteRule> rules;
  for (auto const& rule : request->request().rules()) {
    if (rule.rule_case() == v2::ReadModifyWriteRule::kAppendValue) {
      rules.push_back(ReadModifyWriteRule::AppendValue(
          rule.family_name(), std::string(rule.column_qualifier()),
          std::string(rule.append_value())));
    } else if (rule.rule_case() == v2::ReadModifyWriteRule::kIncrementAmount) {
      rules.push_back(ReadModifyWriteRule::IncrementAmount(
          rule.family_name(), std::string(rule.column_qualifier()),
          rule.increment_amount()));
    } else {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          "Incoming ReadModifyWriteRow request has an unset rule.");
    }
  }
  std::string const& row_key = std::string(request->request().row_key());
  StatusOr<Row> row;
  switch (rules.size()) {
      // API table accepting one or more numbers of ReadModifyWriteRules as
      // parameters. We are assuming that ReadModifyWriteRow wouldn't have too
      // many rules on a single row. Setting the max limit to 5 currently.
    case 1:
      row = table->ReadModifyWriteRow(row_key, rules[0]);
      break;
    case 2:
      row = table->ReadModifyWriteRow(row_key, rules[0], rules[1]);
      break;
    case 3:
      row = table->ReadModifyWriteRow(row_key, rules[0], rules[1], rules[2]);
      break;
    case 4:
      row = table->ReadModifyWriteRow(row_key, rules[0], rules[1], rules[2],
                                      rules[3]);
      break;
    default:
      // Assume maximum 5 rules sent to ReadModifyWriteRow.
      row = table->ReadModifyWriteRow(row_key, rules[0], rules[1], rules[2],
                                      rules[3], rules[4]);
      break;
  }

  *response->mutable_status() = ToRpcStatus(row.status());
  if (!row.ok()) return grpc::Status();
  *response->mutable_row() = ConvertRowToV2(*row);
  return grpc::Status();
}

StatusOr<std::shared_ptr<DataConnection>> CbtTestProxy::GetConnection(
    std::string const& client_id) {
  std::lock_guard<std::mutex> lk(mu_);
  auto client_it = connections_.find(client_id);
  if (client_it == connections_.end()) {
    return google::cloud::internal::NotFoundError(
        absl::StrCat("Client ", client_id, " not found."), GCP_ERROR_INFO());
  }

  return client_it->second;
}

StatusOr<Table> CbtTestProxy::GetTableFromRequest(
    std::string const& client_id, std::string const& table_name) {
  auto tr = MakeTableResource(table_name);
  if (!tr.ok()) return std::move(tr).status();

  auto connection = GetConnection(client_id);
  if (!connection.ok()) return std::move(connection).status();

  return Table(*std::move(connection), *std::move(tr));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace test_proxy
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
