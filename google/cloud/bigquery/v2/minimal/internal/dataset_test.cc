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
  access.dataset.target_types.push_back(std::move(type.value));
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
  access.push_back(MakeAccess("accessrole", "p123", "d123", "t123", "r123",
                              TargetType::Views()));

  std::vector<GcpTag> tags;
  tags.push_back(MakeGcpTag("t1", "t2"));

  Dataset expected;
  expected.kind = "dkind";
  expected.etag = "detag";
  expected.id = "did";
  expected.self_link = "dselflink";
  expected.friendly_name = "dfriendlyname";
  expected.description = "ddescription";
  expected.type = "dtype";
  expected.location = "dlocation";
  expected.default_collation = "ddefaultcollation";
  expected.published = false;
  expected.is_case_insensitive = true;
  expected.labels = labels;
  expected.access = access;
  expected.tags = tags;
  expected.dataset_reference = dataset_ref;
  expected.linked_dataset_source.source_dataset = dataset_ref;
  expected.default_rounding_mode = RoundingMode::RoundHalfEven();
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

  EXPECT_EQ(lhs.default_rounding_mode.value, rhs.default_rounding_mode.value);
  EXPECT_EQ(lhs.storage_billing_model.value, rhs.storage_billing_model.value);
}

std::string MakeDatasetJsonText() {
  return R"({"access":[
    {"dataset":{
         "dataset":{"datasetId":"d123","projectId":"p123"},
         "targetTypes":["VIEWS"]},
         "domain":"",
         "groupByEmail":"",
         "iamMember":"",
         "role":"accessrole",
         "routine":{"datasetId":"d123","projectId":"p123","routineId":"r123"},
         "specialGroup":"",
         "userByEmail":"",
         "view":{"datasetId":"d123","projectId":"p123","tableId":"t123"}
    }],
    "creationTime":"0",
    "datasetReference":{"datasetId":"d123","projectId":"p123"},
    "defaultCollation":"ddefaultcollation",
    "defaultPartitionExpirationMs":"0",
    "defaultRoundingMode":"ROUND_HALF_EVEN",
    "defaultTableExpirationMs":"0",
    "description":"ddescription",
    "etag":"detag",
    "friendlyName":"dfriendlyname",
    "id":"did",
    "isCaseInsensitive":true,
    "kind":"dkind",
    "labels":{"l1":"v1","l2":"v2"},
    "lastModifiedTime":"0",
    "linkedDatasetSource":{"sourceDataset":{
        "datasetId":"d123",
        "projectId":"p123"
    }},
    "location":"dlocation",
    "maxTimeTravelHours":"0",
    "published":false,
    "selfLink":"dselflink",
    "storageBillingModel":"LOGICAL",
    "tags":[{"tagKey":"t1","tagValue":"t2"}],
    "type":"dtype"})";
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
    "kind":"dkind",
    "id":"did",
    "friendlyName":"dfriendlyname",
    "location":"dlocation",
    "type":"DEFAULT",
    "datasetReference": {"projectId":"p123", "datasetId":"d123"},
    "labels":{"l1":"v1","l2":"v2"}
})";
}

ListFormatDataset MakeListFormatDataset() {
  std::map<std::string, std::string> labels;
  labels.insert({"l1", "v1"});
  labels.insert({"l2", "v2"});

  auto dataset_ref = MakeDatasetReference("p123", "d123");

  ListFormatDataset expected;
  expected.kind = "dkind";
  expected.id = "did";
  expected.friendly_name = "dfriendlyname";
  expected.location = "dlocation";
  expected.type = "DEFAULT";
  expected.location = "dlocation";
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

TEST(DatasetTest, DatasetDebugString) {
  Dataset dataset = MakeDataset();

  EXPECT_EQ(
      dataset.DebugString("Dataset", TracingOptions{}),
      R"(Dataset {)"
      R"( kind: "dkind")"
      R"( etag: "detag")"
      R"( id: "did")"
      R"( self_link: "dselflink")"
      R"( friendly_name: "dfriendlyname")"
      R"( description: "ddescription")"
      R"( type: "dtype")"
      R"( location: "dlocation")"
      R"( default_collation: "ddefaultcollation")"
      R"( published: false)"
      R"( is_case_insensitive: true)"
      R"( default_table_expiration { "0" })"
      R"( default_partition_expiration { "0" })"
      R"( creation_time { "1970-01-01T00:00:00Z" })"
      R"( last_modified_time { "1970-01-01T00:00:00Z" })"
      R"( max_time_travel { "0" })"
      R"( labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" })"
      R"( access {)"
      R"( role: "accessrole")"
      R"( user_by_email: "")"
      R"( group_by_email: "")"
      R"( domain: "")"
      R"( special_group: "")"
      R"( iam_member: "")"
      R"( view { project_id: "p123" dataset_id: "d123" table_id: "t123" })"
      R"( routine { project_id: "p123" dataset_id: "d123" routine_id: "r123" })"
      R"( dataset { dataset { project_id: "p123" dataset_id: "d123" })"
      R"( target_types: "VIEWS")"
      R"( })"
      R"( })"
      R"( tags { tag_key: "t1" tag_value: "t2" })"
      R"( dataset_reference { project_id: "p123" dataset_id: "d123" })"
      R"( linked_dataset_source {)"
      R"( source_dataset { project_id: "p123" dataset_id: "d123" })"
      R"( })"
      R"( default_rounding_mode {)"
      R"( value: "ROUND_HALF_EVEN")"
      R"( })"
      R"( storage_billing_model { storage_billing_model_value: "LOGICAL" })"
      R"( })");

  EXPECT_EQ(
      dataset.DebugString(
          "Dataset",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(Dataset {)"
      R"( kind: "dkind")"
      R"( etag: "detag")"
      R"( id: "did")"
      R"( self_link: "dselfli...<truncated>...")"
      R"( friendly_name: "dfriend...<truncated>...")"
      R"( description: "ddescri...<truncated>...")"
      R"( type: "dtype")"
      R"( location: "dlocati...<truncated>...")"
      R"( default_collation: "ddefaul...<truncated>...")"
      R"( published: false)"
      R"( is_case_insensitive: true)"
      R"( default_table_expiration { "0" })"
      R"( default_partition_expiration { "0" })"
      R"( creation_time { "1970-01-01T00:00:00Z" })"
      R"( last_modified_time { "1970-01-01T00:00:00Z" })"
      R"( max_time_travel { "0" })"
      R"( labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" })"
      R"( access {)"
      R"( role: "accessr...<truncated>...")"
      R"( user_by_email: "")"
      R"( group_by_email: "")"
      R"( domain: "")"
      R"( special_group: "")"
      R"( iam_member: "")"
      R"( view { project_id: "p123" dataset_id: "d123" table_id: "t123" })"
      R"( routine { project_id: "p123" dataset_id: "d123" routine_id: "r123" })"
      R"( dataset { dataset { project_id: "p123" dataset_id: "d123" })"
      R"( target_types: "VIEWS")"
      R"( })"
      R"( })"
      R"( tags { tag_key: "t1" tag_value: "t2" })"
      R"( dataset_reference { project_id: "p123" dataset_id: "d123" })"
      R"( linked_dataset_source {)"
      R"( source_dataset { project_id: "p123" dataset_id: "d123" })"
      R"( })"
      R"( default_rounding_mode {)"
      R"( value: "ROUND_H...<truncated>...")"
      R"( })"
      R"( storage_billing_model { storage_billing_model_value: "LOGICAL" })"
      R"( })");

  EXPECT_EQ(dataset.DebugString(
                "Dataset", TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(Dataset {
  kind: "dkind"
  etag: "detag"
  id: "did"
  self_link: "dselflink"
  friendly_name: "dfriendlyname"
  description: "ddescription"
  type: "dtype"
  location: "dlocation"
  default_collation: "ddefaultcollation"
  published: false
  is_case_insensitive: true
  default_table_expiration {
    "0"
  }
  default_partition_expiration {
    "0"
  }
  creation_time {
    "1970-01-01T00:00:00Z"
  }
  last_modified_time {
    "1970-01-01T00:00:00Z"
  }
  max_time_travel {
    "0"
  }
  labels {
    key: "l1"
    value: "v1"
  }
  labels {
    key: "l2"
    value: "v2"
  }
  access {
    role: "accessrole"
    user_by_email: ""
    group_by_email: ""
    domain: ""
    special_group: ""
    iam_member: ""
    view {
      project_id: "p123"
      dataset_id: "d123"
      table_id: "t123"
    }
    routine {
      project_id: "p123"
      dataset_id: "d123"
      routine_id: "r123"
    }
    dataset {
      dataset {
        project_id: "p123"
        dataset_id: "d123"
      }
      target_types: "VIEWS"
    }
  }
  tags {
    tag_key: "t1"
    tag_value: "t2"
  }
  dataset_reference {
    project_id: "p123"
    dataset_id: "d123"
  }
  linked_dataset_source {
    source_dataset {
      project_id: "p123"
      dataset_id: "d123"
    }
  }
  default_rounding_mode {
    value: "ROUND_HALF_EVEN"
  }
  storage_billing_model {
    storage_billing_model_value: "LOGICAL"
  }
})");
}

TEST(DatasetTest, ListFormatDatasetDebugString) {
  auto dataset = MakeListFormatDataset();

  EXPECT_EQ(dataset.DebugString("Dataset", TracingOptions{}),
            R"(Dataset {)"
            R"( kind: "dkind")"
            R"( id: "did")"
            R"( friendly_name: "dfriendlyname")"
            R"( location: "dlocation")"
            R"( type: "DEFAULT")"
            R"( dataset_reference { project_id: "p123" dataset_id: "d123" })"
            R"( labels { key: "l1" value: "v1" })"
            R"( labels { key: "l2" value: "v2" })"
            R"( })");

  EXPECT_EQ(dataset.DebugString("Dataset",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(Dataset {)"
            R"( kind: "dkind")"
            R"( id: "did")"
            R"( friendly_name: "dfriend...<truncated>...")"
            R"( location: "dlocati...<truncated>...")"
            R"( type: "DEFAULT")"
            R"( dataset_reference { project_id: "p123" dataset_id: "d123" })"
            R"( labels { key: "l1" value: "v1" })"
            R"( labels { key: "l2" value: "v2" })"
            R"( })");

  EXPECT_EQ(dataset.DebugString(
                "Dataset", TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(Dataset {
  kind: "dkind"
  id: "did"
  friendly_name: "dfriendlyname"
  location: "dlocation"
  type: "DEFAULT"
  dataset_reference {
    project_id: "p123"
    dataset_id: "d123"
  }
  labels {
    key: "l1"
    value: "v1"
  }
  labels {
    key: "l2"
    value: "v2"
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
