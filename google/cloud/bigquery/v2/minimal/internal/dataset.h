// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_schema.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct StorageBillingModel {
  static StorageBillingModel UnSpecified();
  static StorageBillingModel Logical();
  static StorageBillingModel Physical();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StorageBillingModel, value);

struct TargetType {
  static TargetType UnSpecified();
  static TargetType Views();
  static TargetType Routines();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TargetType, value);

struct LinkedDatasetSource {
  DatasetReference sourceDataset;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LinkedDatasetSource,
                                                sourceDataset);

struct DatasetAccessEntry {
  DatasetReference dataset;
  std::vector<TargetType> targetTypes;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DatasetAccessEntry, dataset,
                                                targetTypes);

struct Access {
  std::string role;
  std::string userByEmail;
  std::string groupByEmail;
  std::string domain;
  std::string specialGroup;
  std::string iamMember;

  TableReference view;
  RoutineReference routine;
  DatasetAccessEntry dataset;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Access, role, userByEmail,
                                                groupByEmail, domain,
                                                specialGroup, iamMember, view,
                                                routine, dataset);

struct GcpTag {
  std::string tagKey;
  std::string tagValue;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GcpTag, tagKey, tagValue);

struct Dataset {
  std::string kind;
  std::string etag;
  std::string id;
  std::string self_link;
  std::string friendly_name;
  std::string description;
  std::string type;
  std::string location;
  std::string default_collation;

  bool published = false;
  bool is_case_insensitive = false;

  std::chrono::milliseconds default_table_expiration =
      std::chrono::milliseconds(0);
  std::chrono::milliseconds default_partition_expiration =
      std::chrono::milliseconds(0);
  std::chrono::system_clock::time_point creation_time;
  std::chrono::system_clock::time_point last_modified_time;
  std::chrono::hours max_time_travel = std::chrono::hours(0);

  std::map<std::string, std::string> labels;
  std::vector<Access> access;
  std::vector<GcpTag> tags;

  DatasetReference dataset_reference;
  LinkedDatasetSource linked_dataset_source;
  RoundingMode default_rounding_mode;

  StorageBillingModel storage_billing_model;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

struct ListFormatDataset {
  std::string kind;
  std::string id;
  std::string friendlyName;
  std::string location;
  std::string type;

  DatasetReference datasetReference;
  std::map<std::string, std::string> labels;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ListFormatDataset, kind, id,
                                                friendlyName, location, type,
                                                datasetReference, labels);

void to_json(nlohmann::json& j, Dataset const& d);
void from_json(nlohmann::json const& j, Dataset& d);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_H
