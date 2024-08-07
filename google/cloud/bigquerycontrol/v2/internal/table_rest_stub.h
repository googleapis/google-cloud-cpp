// Copyright 2024 Google LLC
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

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/bigquery/v2/table.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERYCONTROL_V2_INTERNAL_TABLE_REST_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERYCONTROL_V2_INTERNAL_TABLE_REST_STUB_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/cloud/bigquery/v2/table.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigquerycontrol_v2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TableServiceRestStub {
 public:
  virtual ~TableServiceRestStub() = default;

  virtual StatusOr<google::cloud::bigquery::v2::Table> GetTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::GetTableRequest const& request) = 0;

  virtual StatusOr<google::cloud::bigquery::v2::Table> InsertTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::InsertTableRequest const& request) = 0;

  virtual StatusOr<google::cloud::bigquery::v2::Table> PatchTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::UpdateOrPatchTableRequest const&
          request) = 0;

  virtual StatusOr<google::cloud::bigquery::v2::Table> UpdateTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::UpdateOrPatchTableRequest const&
          request) = 0;

  virtual Status DeleteTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::DeleteTableRequest const& request) = 0;

  virtual StatusOr<google::cloud::bigquery::v2::TableList> ListTables(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::ListTablesRequest const& request) = 0;
};

class DefaultTableServiceRestStub : public TableServiceRestStub {
 public:
  ~DefaultTableServiceRestStub() override = default;

  explicit DefaultTableServiceRestStub(Options options);
  DefaultTableServiceRestStub(
      std::shared_ptr<rest_internal::RestClient> service, Options options);

  StatusOr<google::cloud::bigquery::v2::Table> GetTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::GetTableRequest const& request) override;

  StatusOr<google::cloud::bigquery::v2::Table> InsertTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::InsertTableRequest const& request) override;

  StatusOr<google::cloud::bigquery::v2::Table> PatchTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::UpdateOrPatchTableRequest const& request)
      override;

  StatusOr<google::cloud::bigquery::v2::Table> UpdateTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::UpdateOrPatchTableRequest const& request)
      override;

  Status DeleteTable(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::DeleteTableRequest const& request) override;

  StatusOr<google::cloud::bigquery::v2::TableList> ListTables(
      google::cloud::rest_internal::RestContext& rest_context,
      Options const& options,
      google::cloud::bigquery::v2::ListTablesRequest const& request) override;

 private:
  std::shared_ptr<rest_internal::RestClient> service_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquerycontrol_v2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERYCONTROL_V2_INTERNAL_TABLE_REST_STUB_H
