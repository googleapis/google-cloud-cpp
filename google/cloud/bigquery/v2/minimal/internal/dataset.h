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

#include "google/cloud/version.h"
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
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StorageBillingModel, value);

struct TargetType {
  static TargetType UnSpecified();
  static TargetType Views();
  static TargetType Routines();

  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TargetType, value);

struct TableFieldSchemaRoundingMode {
  static TableFieldSchemaRoundingMode UnSpecified();
  static TableFieldSchemaRoundingMode RoundHalfAwayFromZero();
  static TableFieldSchemaRoundingMode RoundHalfEven();

  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TableFieldSchemaRoundingMode,
                                                value);

struct DatasetReference {
  std::string dataset_id;
  std::string project_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DatasetReference, dataset_id,
                                                project_id);

struct LinkedDatasetSource {
  DatasetReference source_dataset;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LinkedDatasetSource,
                                                source_dataset);

struct TableReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TableReference, dataset_id,
                                                project_id, table_id);

struct RoutineReference {
  std::string project_id;
  std::string dataset_id;
  std::string routine_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RoutineReference, dataset_id,
                                                project_id, routine_id);

struct DatasetAccessEntry {
  DatasetReference dataset;
  std::vector<TargetType> target_types;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DatasetAccessEntry, dataset,
                                                target_types);

struct Access {
  std::string role;
  std::string user_by_email;
  std::string group_by_email;
  std::string domain;
  std::string special_group;
  std::string iam_member;

  TableReference view;
  RoutineReference routine;
  DatasetAccessEntry dataset;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Access, role, user_by_email,
                                                group_by_email, domain,
                                                special_group, iam_member, view,
                                                routine, dataset);

struct HiveMetastoreConnectivity {
  std::string access_uri_type;
  std::string access_uri;
  std::string metadata_connection;
  std::string storage_connection;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HiveMetastoreConnectivity,
                                                access_uri_type, access_uri,
                                                metadata_connection,
                                                storage_connection);

struct HiveDatabaseReference {
  std::string catalog_id;
  std::string database;

  HiveMetastoreConnectivity metadata_connectivity;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HiveDatabaseReference,
                                                catalog_id, database,
                                                metadata_connectivity);

struct ExternalDatasetReference {
  HiveDatabaseReference hive_database;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ExternalDatasetReference,
                                                hive_database);

struct GcpTag {
  std::string tag_key;
  std::string tag_value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GcpTag, tag_key, tag_value);

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

  std::chrono::milliseconds default_table_expiration_ms =
      std::chrono::milliseconds(0);
  std::chrono::milliseconds default_partition_expiration_ms =
      std::chrono::milliseconds(0);
  std::chrono::system_clock::time_point creation_time;
  std::chrono::system_clock::time_point last_modified_time;
  ;
  std::chrono::hours max_time_travel_hours = std::chrono::hours(0);

  std::map<std::string, std::string> labels;
  std::vector<Access> access;
  std::vector<GcpTag> tags;

  DatasetReference dataset_reference;
  LinkedDatasetSource linked_dataset_source;
  ExternalDatasetReference external_dataset_reference;
  TableFieldSchemaRoundingMode default_rounding_mode;

  StorageBillingModel storage_billing_model;
};

void to_json(nlohmann::json& j, Dataset const& d);
void from_json(nlohmann::json const& j, Dataset& d);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_H
