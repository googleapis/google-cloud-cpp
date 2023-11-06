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

#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::IsNull;
using ::testing::Not;

TEST(JsonUtilsTest, FromJsonMillisecondsNumber) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":10})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::milliseconds field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::milliseconds(10));
}

TEST(JsonUtilsTest, FromJsonMillisecondsString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::milliseconds field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::milliseconds(10));
}

TEST(JsonUtilsTest, ToJsonMillisecondsString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto expected_json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto field = std::chrono::milliseconds{10};
  nlohmann::json actual_json;
  ToJson(field, actual_json, name);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(JsonUtilsTest, ToJsonMillisecondsNumber) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":10})";
  auto expected_json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto field = std::chrono::milliseconds{10};
  nlohmann::json actual_json;
  ToIntJson(field, actual_json, name);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(JsonUtilsTest, FromJsonHoursNumber) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":10})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::hours field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::hours(10));
}

TEST(JsonUtilsTest, FromJsonHoursString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::hours field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::hours(10));
}

TEST(JsonUtilsTest, ToJsonHoursString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto expected_json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto field = std::chrono::hours{10};
  nlohmann::json actual_json;
  ToJson(field, actual_json, name);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(JsonUtilsTest, FromJsonTimepointNumber) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":10})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::system_clock::time_point field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::system_clock::time_point{
                       std::chrono::milliseconds(10)});
}

TEST(JsonUtilsTest, FromJsonTimepointString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::chrono::system_clock::time_point field;
  FromJson(field, json, name);

  EXPECT_EQ(field, std::chrono::system_clock::time_point{
                       std::chrono::milliseconds(10)});
}

TEST(JsonUtilsTest, ToJsonTimepointString) {
  auto const* const name = "start_time";
  auto constexpr kJsonText = R"({"start_time":"10"})";
  auto expected_json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto field =
      std::chrono::system_clock::time_point{std::chrono::milliseconds(10)};
  nlohmann::json actual_json;
  ToJson(field, actual_json, name);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(JsonUtilsTest, SafeGetToCustomType) {
  auto const* const key = "error_result";
  auto constexpr kJsonText =
      R"({"error_result":{
    "reason":"testing",
    "location":"us-east",
    "message":"testing"
  }})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  ErrorProto actual;
  SafeGetTo(actual, json, key);

  ErrorProto expected;
  expected.reason = "testing";
  expected.location = "us-east";
  expected.message = "testing";

  EXPECT_EQ(expected, actual);
}

TEST(JsonUtilsTest, SafeGetToSharedPtrKeyPresent) {
  auto const* const key = "project_id";
  auto constexpr kJsonText = R"({"project_id":"123"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::shared_ptr<std::string> val;
  EXPECT_TRUE(SafeGetTo(val, json, key));
  EXPECT_THAT(val, Not(IsNull()));
  EXPECT_EQ(*val, "123");
}

TEST(JsonUtilsTest, SafeGetToSharedPtrKeyAbsent) {
  auto const* const key = "job_id";
  auto constexpr kJsonText = R"({"project_id":"123"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::shared_ptr<std::string> val;
  EXPECT_FALSE(SafeGetTo(val, json, key));
  EXPECT_THAT(val, IsNull());
}

TEST(JsonUtilsTest, SafeGetToKeyPresent) {
  auto const* const key = "project_id";
  auto constexpr kJsonText = R"({"project_id":"123"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::string val;
  EXPECT_TRUE(SafeGetTo(val, json, key));
  EXPECT_EQ(val, "123");
}

TEST(JsonUtilsTest, SafeGetToKeyAbsent) {
  auto const* const key = "job_id";
  auto constexpr kJsonText = R"({"project_id":"123"})";
  auto json = nlohmann::json::parse(kJsonText, nullptr, false);
  EXPECT_TRUE(json.is_object());

  std::string val;
  EXPECT_FALSE(SafeGetTo(val, json, key));
}

TEST(JsonUtilsTest, RemoveKeys) {
  std::vector<std::string> keys = {"start_time", "dataset_id"};
  auto constexpr kJsonText =
      R"({"start_time":"10", "project_id": "1", "dataset_id":"1"})";

  auto json = RemoveJsonKeysAndEmptyFields(kJsonText, keys);
  auto const* expected = R"({"project_id":"1"})";

  EXPECT_EQ(expected, json.dump());
}

TEST(JsonUtilsTest, RemoveEmptyObjects) {
  std::vector<std::string> keys = {"start_time", "query"};
  auto constexpr kJsonText =
      R"({"start_time":"10", "project_id": "1", "query":{}})";

  auto json = RemoveJsonKeysAndEmptyFields(kJsonText, keys);
  auto const* expected = R"({"project_id":"1"})";

  EXPECT_EQ(expected, json.dump());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
