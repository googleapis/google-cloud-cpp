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

#include "google/cloud/bigquery/v2/minimal/internal/dataset.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::IsEmpty;
using ::testing::Not;

// Helper functions.
namespace {

DatasetReference MakeDatasetReference(std::string project_id,
                                      std::string dataset_id) {
  DatasetReference dataset;
  dataset.project_id = std::move(project_id);
  dataset.dataset_id = std::move(dataset_id);
  return dataset;
}

Access MakeAccess(std::string role, std::string project_id,
                  std::string dataset_id, std::string table_id,
                  std::string routine_id, TargetType type) {
  Access access;
  access.role = std::move(role);
  access.view.project_id = project_id;
  access.view.dataset_id = dataset_id;
  access.view.table_id = std::move(table_id);
  access.routine.project_id = project_id;
  access.routine.dataset_id = dataset_id;
  access.routine.routine_id = std::move(routine_id);
  access.dataset.dataset =
      MakeDatasetReference(std::move(project_id), std::move(dataset_id));
  access.dataset.target_types.push_back(std::move(type));
  return access;
}

GcpTag MakeGcpTag(std::string key, std::string value) {
  GcpTag tag;
  tag.tag_key = std::move(key);
  tag.tag_value = std::move(value);
  return tag;
}

Dataset MakeDataset() {
  std::map<std::string, std::string> labels;
  labels.insert({"l1", "v1"});
  labels.insert({"l2", "v2"});

  auto dataset_ref = MakeDatasetReference("p123", "d123");

  std::vector<Access> access;
  access.push_back(MakeAccess("access-role", "p123", "d123", "t123", "r123",
                              TargetType::Views()));

  std::vector<GcpTag> tags;
  tags.push_back(MakeGcpTag("t1", "t2"));

  Dataset expected;
  expected.kind = "d-kind";
  expected.etag = "d-etag";
  expected.id = "d-id";
  expected.self_link = "d-self-link";
  expected.friendly_name = "d-friendly-name";
  expected.description = "d-description";
  expected.type = "d-type";
  expected.location = "d-location";
  expected.default_collation = "d-default-collation";
  expected.published = false;
  expected.is_case_insensitive = true;
  expected.labels = labels;
  expected.access = access;
  expected.tags = tags;
  expected.dataset_reference = dataset_ref;
  expected.linked_dataset_source.source_dataset = dataset_ref;
  expected.external_dataset_reference.hive_database.catalog_id = "c1";
  expected.external_dataset_reference.hive_database.database = "d1";
  expected.default_rounding_mode =
      TableFieldSchemaRoundingMode::RoundHalfEven();
  expected.storage_billing_model = StorageBillingModel::Logical();

  return expected;
}

void AssertEquals(Dataset const& lhs, Dataset const& rhs) {
  EXPECT_EQ(lhs.kind, rhs.kind);
  EXPECT_EQ(lhs.etag, rhs.etag);
  EXPECT_EQ(lhs.id, rhs.id);
  EXPECT_EQ(lhs.self_link, rhs.self_link);
  EXPECT_EQ(lhs.friendly_name, rhs.friendly_name);
  EXPECT_EQ(lhs.description, rhs.description);
  EXPECT_EQ(lhs.type, rhs.type);
  EXPECT_EQ(lhs.location, rhs.location);
  EXPECT_EQ(lhs.default_collation, rhs.default_collation);
  EXPECT_EQ(lhs.published, rhs.published);
  EXPECT_EQ(lhs.is_case_insensitive, rhs.is_case_insensitive);

  ASSERT_THAT(lhs.labels, Not(IsEmpty()));
  ASSERT_THAT(rhs.labels, Not(IsEmpty()));
  EXPECT_EQ(lhs.labels.size(), rhs.labels.size());
  EXPECT_EQ(lhs.labels.find("l1")->second, rhs.labels.find("l1")->second);
  EXPECT_EQ(lhs.labels.find("l2")->second, rhs.labels.find("l2")->second);

  ASSERT_THAT(lhs.access, Not(IsEmpty()));
  ASSERT_THAT(rhs.access, Not(IsEmpty()));
  EXPECT_EQ(lhs.access.size(), rhs.access.size());
  EXPECT_EQ(lhs.access[0].role, rhs.access[0].role);
  EXPECT_EQ(lhs.access[0].domain, rhs.access[0].domain);
  EXPECT_EQ(lhs.access[0].group_by_email, rhs.access[0].group_by_email);
  EXPECT_EQ(lhs.access[0].iam_member, rhs.access[0].iam_member);
  EXPECT_EQ(lhs.access[0].special_group, rhs.access[0].special_group);
  EXPECT_EQ(lhs.access[0].user_by_email, rhs.access[0].user_by_email);

  EXPECT_EQ(lhs.access[0].view.dataset_id, rhs.access[0].view.dataset_id);
  EXPECT_EQ(lhs.access[0].view.project_id, rhs.access[0].view.project_id);
  EXPECT_EQ(lhs.access[0].view.table_id, rhs.access[0].view.table_id);

  EXPECT_EQ(lhs.access[0].routine.dataset_id, rhs.access[0].routine.dataset_id);
  EXPECT_EQ(lhs.access[0].routine.project_id, rhs.access[0].routine.project_id);
  EXPECT_EQ(lhs.access[0].routine.routine_id, rhs.access[0].routine.routine_id);

  ASSERT_THAT(lhs.tags, Not(IsEmpty()));
  ASSERT_THAT(rhs.tags, Not(IsEmpty()));
  EXPECT_EQ(lhs.tags.size(), rhs.tags.size());
  EXPECT_EQ(lhs.tags[0].tag_key, rhs.tags[0].tag_key);
  EXPECT_EQ(lhs.tags[0].tag_value, rhs.tags[0].tag_value);

  EXPECT_EQ(lhs.dataset_reference.dataset_id, rhs.dataset_reference.dataset_id);
  EXPECT_EQ(lhs.dataset_reference.project_id, rhs.dataset_reference.project_id);

  EXPECT_EQ(lhs.linked_dataset_source.source_dataset.dataset_id,
            rhs.linked_dataset_source.source_dataset.dataset_id);
  EXPECT_EQ(lhs.linked_dataset_source.source_dataset.project_id,
            rhs.linked_dataset_source.source_dataset.project_id);

  EXPECT_EQ(lhs.external_dataset_reference.hive_database.catalog_id,
            rhs.external_dataset_reference.hive_database.catalog_id);

  EXPECT_EQ(lhs.external_dataset_reference.hive_database.database,
            rhs.external_dataset_reference.hive_database.database);

  EXPECT_EQ(lhs.default_rounding_mode.value, rhs.default_rounding_mode.value);
  EXPECT_EQ(lhs.storage_billing_model.value, rhs.storage_billing_model.value);
}

std::string MakeDatasetJsonText() {
  return R"({"access":[
    {"dataset":{
         "dataset":{"dataset_id":"d123","project_id":"p123"},
         "target_types":[{"value":"VIEWS"}]},
         "domain":"","group_by_email":"",
         "iam_member":"",
         "role":"access-role",
         "routine":{"dataset_id":"d123","project_id":"p123","routine_id":"r123"},
         "special_group":"",
         "user_by_email":"",
         "view":{"dataset_id":"d123","project_id":"p123","table_id":"t123"}
    }],
    "creation_time":0,
    "dataset_reference":{"dataset_id":"d123","project_id":"p123"},
    "default_collation":"d-default-collation",
    "default_partition_expiration":0,
    "default_rounding_mode":{"value":"ROUND_HALF_EVEN"},
    "default_table_expiration":0,
    "description":"d-description",
    "etag":"d-etag",
    "external_dataset_reference":{
        "hive_database":{
            "catalog_id":"c1",
            "database":"d1",
            "metadata_connectivity":{
                "access_uri":"",
                "access_uri_type":"",
                "metadata_connection":"",
                "storage_connection":""
            }
       }
    },
    "friendly_name":"d-friendly-name",
    "id":"d-id",
    "is_case_insensitive":true,
    "kind":"d-kind",
    "labels":{"l1":"v1","l2":"v2"},
    "last_modified_time":0,
    "linked_dataset_source":{"source_dataset":{
        "dataset_id":"d123",
        "project_id":"p123"
    }},
    "location":"d-location",
    "max_time_travel":0,
    "published":false,
    "self_link":"d-self-link",
    "storage_billing_model":{"value":"LOGICAL"},
    "tags":[{"tag_key":"t1","tag_value":"t2"}],
    "type":"d-type"})";
}

void AssertEquals(ListFormatDataset const& lhs, ListFormatDataset const& rhs) {
  EXPECT_EQ(lhs.kind, rhs.kind);
  EXPECT_EQ(lhs.id, rhs.id);
  EXPECT_EQ(lhs.friendly_name, rhs.friendly_name);
  EXPECT_EQ(lhs.type, rhs.type);
  EXPECT_EQ(lhs.location, rhs.location);

  ASSERT_THAT(lhs.labels, Not(IsEmpty()));
  ASSERT_THAT(rhs.labels, Not(IsEmpty()));
  EXPECT_EQ(lhs.labels.size(), rhs.labels.size());
  EXPECT_EQ(lhs.labels.find("l1")->second, rhs.labels.find("l1")->second);
  EXPECT_EQ(lhs.labels.find("l2")->second, rhs.labels.find("l2")->second);

  EXPECT_EQ(lhs.dataset_reference.dataset_id, rhs.dataset_reference.dataset_id);
  EXPECT_EQ(lhs.dataset_reference.project_id, rhs.dataset_reference.project_id);
}

std::string MakeListFormatDatasetJsonText() {
  return R"({
    "kind":"d-kind",
    "id":"d-id",
    "friendly_name":"d-friendly-name",
    "location":"d-location",
    "type":"DEFAULT",
    "dataset_reference": {"project_id":"p123", "dataset_id":"d123"},
    "labels":{"l1":"v1","l2":"v2"}
})";
}

ListFormatDataset MakeListFormatDataset() {
  std::map<std::string, std::string> labels;
  labels.insert({"l1", "v1"});
  labels.insert({"l2", "v2"});

  auto dataset_ref = MakeDatasetReference("p123", "d123");

  ListFormatDataset expected;
  expected.kind = "d-kind";
  expected.id = "d-id";
  expected.friendly_name = "d-friendly-name";
  expected.location = "d-location";
  expected.type = "DEFAULT";
  expected.location = "d-location";
  expected.labels = labels;
  expected.dataset_reference = dataset_ref;

  return expected;
}

}  // namespace

TEST(DatasetTest, DatasetToJson) {
  auto const text = MakeDatasetJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input_dataset = MakeDataset();

  nlohmann::json actual_json;
  to_json(actual_json, input_dataset);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(DatasetTest, DatasetFromJson) {
  auto const text = MakeDatasetJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  Dataset expected_dataset = MakeDataset();

  Dataset actual_dataset;
  from_json(json, actual_dataset);

  AssertEquals(expected_dataset, actual_dataset);
}

TEST(DatasetTest, ListFormatDatasetToJson) {
  auto const text = MakeListFormatDatasetJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input_dataset = MakeListFormatDataset();

  nlohmann::json actual_json;
  to_json(actual_json, input_dataset);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(DatasetTest, ListFormatDatasetFromJson) {
  auto const text = MakeListFormatDatasetJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  auto expected_dataset = MakeListFormatDataset();

  ListFormatDataset actual_dataset;
  from_json(json, actual_dataset);

  AssertEquals(expected_dataset, actual_dataset);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
