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

#include "generator/internal/discovery_to_proto.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Key;

TEST(ExtractTypesFromSchema, Success) {
  auto constexpr kDiscoveryDocWithCorrectSchema = R"""(
{
  "schemas": {
    "Foo": {
      "id": "Foo",
      "type": "object"
    },
    "Bar": {
      "id": "Bar",
      "type": "object"
    }
  }
}
)""";

  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithCorrectSchema, nullptr, false);
  ASSERT_FALSE(parsed_json.is_discarded() || parsed_json.is_null());
  auto types = ExtractTypesFromSchema(parsed_json);
  ASSERT_THAT(types, IsOk());
  EXPECT_THAT(*types, ::testing::UnorderedElementsAre(Key("Foo"), Key("Bar")));
  EXPECT_THAT(*types, Contains(Key(Eq(std::string("Foo")))));
  EXPECT_THAT(*types, Contains(Key(Eq(std::string("Bar")))));
}

TEST(ExtractTypesFromSchema, MissingSchema) {
  auto constexpr kDiscoveryDocMissingSchema = R"""(
{
}
)""";

  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocMissingSchema, nullptr, false);
  ASSERT_FALSE(parsed_json.is_discarded() || parsed_json.is_null());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("does not contain schemas element")));
}

TEST(ExtractTypesFromSchema, SchemaIdMissing) {
  auto constexpr kDiscoveryDocSchemaIdMissing = R"""(
{
  "schemas": {
    "NoId": {
      "type": "object"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocSchemaIdMissing, nullptr, false);
  ASSERT_FALSE(parsed_json.is_discarded() || parsed_json.is_null());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("schema without id")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("schema has no id")));
}

TEST(ExtractTypesFromSchema, SchemaMissingType) {
  auto constexpr kDiscoveryDocWithMissingType = R"""(
{
  "schemas": {
    "MissingType": {
      "id": "MissingType"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithMissingType, nullptr, false);
  ASSERT_FALSE(parsed_json.is_discarded() || parsed_json.is_null());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("MissingType not type:object is untyped")));
}

TEST(ExtractTypesFromSchema, SchemaNonObject) {
  auto constexpr kDiscoveryDocWithNonObjectSchema = R"""(
{
  "schemas": {
    "NonObject": {
      "id": "NonObject",
      "type": "array"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithNonObjectSchema, nullptr, false);
  ASSERT_FALSE(parsed_json.is_discarded() || parsed_json.is_null());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("NonObject not type:object is array")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
