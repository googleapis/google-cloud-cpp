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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_IMPL_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/data_connection.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigtable::Row TransformReadModifyWriteRowResponse(
    google::bigtable::v2::ReadModifyWriteRowResponse response);

class DataConnectionImpl : public DataConnection {
 public:
  ~DataConnectionImpl() override = default;

  DataConnectionImpl(std::unique_ptr<BackgroundThreads> background,
                     std::shared_ptr<BigtableStub> stub, Options options);

  Options options() override { return options_; }

  Status Apply(std::string const& app_profile_id, std::string const& table_name,
               bigtable::SingleRowMutation mut) override;

  future<Status> AsyncApply(std::string const& app_profile_id,
                            std::string const& table_name,
                            bigtable::SingleRowMutation mut) override;

  std::vector<bigtable::FailedMutation> BulkApply(
      std::string const& app_profile_id, std::string const& table_name,
      bigtable::BulkMutation mut) override;

  future<std::vector<bigtable::FailedMutation>> AsyncBulkApply(
      std::string const& app_profile_id, std::string const& table_name,
      bigtable::BulkMutation mut) override;

  bigtable::RowReader ReadRows(std::string const& app_profile_id,
                               std::string const& table_name,
                               bigtable::RowSet row_set,
                               std::int64_t rows_limit,
                               bigtable::Filter filter) override;

  StatusOr<std::pair<bool, bigtable::Row>> ReadRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter) override;

  StatusOr<bigtable::MutationBranch> CheckAndMutateRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter,
      std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations) override;

  future<StatusOr<bigtable::MutationBranch>> AsyncCheckAndMutateRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter,
      std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations) override;

  StatusOr<std::vector<bigtable::RowKeySample>> SampleRows(
      std::string const& app_profile_id,
      std::string const& table_name) override;

  future<StatusOr<std::vector<bigtable::RowKeySample>>> AsyncSampleRows(
      std::string const& app_profile_id,
      std::string const& table_name) override;

  StatusOr<bigtable::Row> ReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request) override;

  future<StatusOr<bigtable::Row>> AsyncReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request) override;

  void AsyncReadRows(std::string const& app_profile_id,
                     std::string const& table_name, bigtable::RowFunctor on_row,
                     bigtable::FinishFunctor on_finish,
                     bigtable::RowSet row_set, std::int64_t rows_limit,
                     bigtable::Filter filter) override;

  future<StatusOr<std::pair<bool, bigtable::Row>>> AsyncReadRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter) override;

 private:
  std::unique_ptr<DataRetryPolicy> retry_policy() {
    auto const& options = internal::CurrentOptions();
    if (options.has<DataRetryPolicyOption>()) {
      return options.get<DataRetryPolicyOption>()->clone();
    }
    return options_.get<DataRetryPolicyOption>()->clone();
  }

  std::unique_ptr<BackoffPolicy> backoff_policy() {
    auto const& options = internal::CurrentOptions();
    if (options.has<DataBackoffPolicyOption>()) {
      return options.get<DataBackoffPolicyOption>()->clone();
    }
    return options_.get<DataBackoffPolicyOption>()->clone();
  }

  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency_policy() {
    auto const& options = internal::CurrentOptions();
    if (options.has<IdempotentMutationPolicyOption>()) {
      return options.get<IdempotentMutationPolicyOption>()->clone();
    }
    return options_.get<IdempotentMutationPolicyOption>()->clone();
  }

  std::unique_ptr<BackgroundThreads> background_;
  std::shared_ptr<BigtableStub> stub_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_IMPL_H
