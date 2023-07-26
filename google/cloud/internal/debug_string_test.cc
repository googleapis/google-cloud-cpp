// Copyright 2022 Google LLC
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

#include "google/cloud/internal/debug_string.h"
#include "absl/strings/string_view.h"
#include <gmock/gmock.h>
#include <chrono>
#include <map>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

struct SubMessage {
  double sub_field;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const {
    return DebugFormatter(name, options, indent)
        .Field("sub_field", sub_field)
        .Build();
  }
};

TEST(DebugFormatter, SingleLine) {
  EXPECT_EQ(DebugFormatter("message_name",
                           TracingOptions{}.SetOptions("single_line_mode=T"))
                .Field("field1", 42)
                .SubMessage("sub_message", SubMessage{3.14159})
                .StringField("field2", "foobar")
                .Field("field3", true)
                .Build(),
            R"(message_name {)"
            R"( field1: 42)"
            R"( sub_message {)"
            R"( sub_field: 3.14159)"
            R"( })"
            R"( field2: "foobar")"
            R"( field3: true)"
            R"( })");
}

TEST(DebugFormatter, MultiLine) {
  EXPECT_EQ(DebugFormatter("message_name",
                           TracingOptions{}.SetOptions("single_line_mode=F"))
                .Field("field1", 42)
                .SubMessage("sub_message", SubMessage{3.14159})
                .StringField("field2", "foobar")
                .Field("field3", true)
                .Build(),
            R"(message_name {
  field1: 42
  sub_message {
    sub_field: 3.14159
  }
  field2: "foobar"
  field3: true
})");
}

TEST(DebugFormatter, Truncated) {
  EXPECT_EQ(
      DebugFormatter("message_name", TracingOptions{}.SetOptions(
                                         "truncate_string_field_longer_than=3"))
          .Field("field1", 42)
          .SubMessage("sub_message", SubMessage{3.14159})
          .StringField("field2", "foobar")
          .Field("field3", true)
          .Build(),
      R"(message_name {)"
      R"( field1: 42)"
      R"( sub_message {)"
      R"( sub_field: 3.14159)"
      R"( })"
      R"( field2: "foo...<truncated>...")"
      R"( field3: true)"
      R"( })");
}

TEST(DebugFormatter, TimePoint) {
  absl::optional<std::chrono::system_clock::time_point> tp =
      std::chrono::system_clock::from_time_t(1681165293) +
      std::chrono::microseconds(123456);
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", *tp)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( "2023-04-10T22:21:33.123456Z")"
            R"( })"
            R"( })");
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", tp)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( "2023-04-10T22:21:33.123456Z")"
            R"( })"
            R"( })");
  tp = absl::nullopt;
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", tp)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, Duration) {
  auto d = std::chrono::microseconds(123456);
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", d)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( "123.456ms")"
            R"( })"
            R"( })");
}

TEST(DebugFormatter, Map) {
  std::map<std::string, SubMessage> m = {{"k1", SubMessage{3.1}},
                                         {"k2", SubMessage{4.2}}};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( key: "k1")"
            R"( value {)"
            R"( sub_field: 3.1)"
            R"( })"
            R"( })"
            R"( field1 {)"
            R"( key: "k2")"
            R"( value {)"
            R"( sub_field: 4.2)"
            R"( })"
            R"( })"
            R"( })");
  m.clear();
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, MapString) {
  std::map<std::string, std::string> m = {{"k1", "v1"}, {"k2", "v2"}};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( key: "k1")"
            R"( value: "v1")"
            R"( })"
            R"( field1 {)"
            R"( key: "k2")"
            R"( value: "v2")"
            R"( })"
            R"( })");
  m.clear();
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, Multimap) {
  std::multimap<std::string, std::string> m = {{"k1", "v1"}, {"k1", "v2"}};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( key: "k1")"
            R"( value: "v1")"
            R"( })"
            R"( field1 {)"
            R"( key: "k1")"
            R"( value: "v2")"
            R"( })"
            R"( })");
  m.clear();
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, Optional) {
  absl::optional<SubMessage> m = SubMessage{3.14159};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( sub_field: 3.14159)"
            R"( })"
            R"( })");
  m = absl::nullopt;
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", m)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, Vector) {
  std::vector<SubMessage> v = {SubMessage{1}, SubMessage{2}, SubMessage{3}};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", v)
                .Build(),
            R"(message_name {)"
            R"( field1 {)"
            R"( sub_field: 1)"
            R"( })"
            R"( field1 {)"
            R"( sub_field: 2)"
            R"( })"
            R"( field1 {)"
            R"( sub_field: 3)"
            R"( })"
            R"( })");
  v.clear();
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", v)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugFormatter, VectorString) {
  std::vector<std::string> v = {"foo", "bar", "baz"};
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", v)
                .Build(),
            R"(message_name {)"
            R"( field1: "foo")"
            R"( field1: "bar")"
            R"( field1: "baz")"
            R"( })");
  v.clear();
  EXPECT_EQ(DebugFormatter("message_name", TracingOptions{})
                .Field("field1", v)
                .Build(),
            R"(message_name {)"
            R"( })");
}

TEST(DebugString, TruncateString) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  struct Case {
    std::string actual;
    std::string expected;
  } cases[] = {
      {"1234567", "1234567"},
      {"12345678", "12345678"},
      {"123456789", "12345678...<truncated>..."},
      {"1234567890", "12345678...<truncated>..."},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(c.expected, DebugString(c.actual, tracing_options));
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
