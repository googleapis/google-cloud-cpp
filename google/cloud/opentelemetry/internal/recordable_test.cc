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

#include "google/cloud/opentelemetry/internal/recordable.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::devtools::cloudtrace::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

auto constexpr kProjectId = "test-project";

template <typename T>
opentelemetry::nostd::span<T> MakeCompositeAttribute() {
  T v[]{T{}, T{}};
  return opentelemetry::nostd::span<T>(v);
}

TEST(SetTruncatableString, LessThanLimit) {
  v2::TruncatableString proto;
  SetTruncatableString(proto, "value", 1000);
  EXPECT_EQ(proto.value(), "value");
  EXPECT_EQ(proto.truncated_byte_count(), 0);
}

TEST(SetTruncatableString, OverTheLimit) {
  v2::TruncatableString proto;
  SetTruncatableString(proto, "abcde", 3);
  EXPECT_EQ(proto.value(), "abc");
  EXPECT_EQ(proto.truncated_byte_count(), 2);
}

TEST(SetTruncatableString, RespectsUnicodeSymbolBoundaries) {
  v2::TruncatableString proto;
  // A UTF-8 encoded character that is 2 bytes wide.
  std::string const u2 = "\xd0\xb4";
  // The string `u2 + u2` is 4 bytes long. Truncation should respect the symbol
  // boundaries. i.e. we should not cut the symbol in half.
  SetTruncatableString(proto, u2 + u2, 3);
  EXPECT_EQ(proto.value(), u2);
  EXPECT_EQ(proto.truncated_byte_count(), 2);

  // A UTF-8 encoded character that is 3 bytes wide.
  std::string const u3 = "\xe6\x96\xad";
  SetTruncatableString(proto, u3 + u3, 5);
  EXPECT_EQ(proto.value(), u3);
  EXPECT_EQ(proto.truncated_byte_count(), 3);

  // Test the empty case.
  SetTruncatableString(proto, u3 + u3, 2);
  EXPECT_THAT(proto.value(), IsEmpty());
  EXPECT_EQ(proto.truncated_byte_count(), 6);
}

TEST(AddAttribute, DropsNewKeyAtLimit) {
  v2::Span::Attributes attributes;
  AddAttribute(attributes, "accepted", "value", 1);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(), ElementsAre(Pair("accepted", _)));

  AddAttribute(attributes, "rejected", "value", 1);
  EXPECT_EQ(attributes.dropped_attributes_count(), 1);
  EXPECT_THAT(attributes.attribute_map(), ElementsAre(Pair("accepted", _)));
}

TEST(AddAttribute, AcceptsExistingKeyAtLimit) {
  v2::AttributeValue expected_1;
  *expected_1.mutable_string_value()->mutable_value() = "value1";
  v2::AttributeValue expected_2;
  *expected_2.mutable_string_value()->mutable_value() = "value2";

  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", "value1", 1);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected_1))));

  // The map is full, but we should still be able to overwrite an existing key.
  AddAttribute(attributes, "key", "value2", 1);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected_2))));
}

TEST(AddAttribute, DropsLongKey) {
  std::string const long_key(kAttributeKeyStringLimit + 1, 'A');

  v2::Span::Attributes attributes;
  AddAttribute(attributes, long_key, "value", 32);
  EXPECT_EQ(attributes.dropped_attributes_count(), 1);
  EXPECT_THAT(attributes.attribute_map(), IsEmpty());
}

TEST(AddAttribute, HandlesBoolAttribute) {
  v2::AttributeValue expected;
  expected.set_bool_value(true);

  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", true, 32);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected))));
}

TEST(AddAttribute, HandlesIntAttributes) {
  v2::AttributeValue expected;
  expected.set_int_value(42);

  std::vector<opentelemetry::common::AttributeValue> values = {
      std::int32_t{42}, std::uint32_t{42}, std::int64_t{42}, std::uint64_t{42}};

  for (auto const& value : values) {
    v2::Span::Attributes attributes;
    AddAttribute(attributes, "key", value, 32);
    EXPECT_EQ(attributes.dropped_attributes_count(), 0);
    EXPECT_THAT(attributes.attribute_map(),
                ElementsAre(Pair("key", IsProtoEqual(expected))));
  }
}

TEST(AddAttribute, HandlesStringAttributes) {
  v2::AttributeValue expected;
  expected.mutable_string_value()->set_value("value");

  std::vector<opentelemetry::common::AttributeValue> values = {
      "value", opentelemetry::nostd::string_view{"value"}};

  for (auto const& value : values) {
    v2::Span::Attributes attributes;
    AddAttribute(attributes, "key", value, 32);
    EXPECT_EQ(attributes.dropped_attributes_count(), 0);
    EXPECT_THAT(attributes.attribute_map(),
                ElementsAre(Pair("key", IsProtoEqual(expected))));
  }
}

TEST(AddAttribute, TruncatesStringValue) {
  std::string const long_value(kAttributeValueStringLimit + 1, 'A');
  std::string const expected_value(kAttributeValueStringLimit, 'A');

  v2::AttributeValue expected;
  expected.mutable_string_value()->set_value(expected_value);
  expected.mutable_string_value()->set_truncated_byte_count(1);

  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", long_value, 32);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected))));
}

TEST(AddAttribute, ConvertsDoubleAttributeToString) {
  v2::AttributeValue expected;
  expected.mutable_string_value()->set_value("4.2");

  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", 4.2, 32);
  EXPECT_EQ(attributes.dropped_attributes_count(), 0);
  EXPECT_THAT(attributes.attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected))));
}

TEST(AddAttribute, DropsCompositeAttributes) {
  std::vector<opentelemetry::common::AttributeValue> values = {
      MakeCompositeAttribute<bool>(),
      MakeCompositeAttribute<std::int32_t>(),
      MakeCompositeAttribute<std::int64_t>(),
      MakeCompositeAttribute<std::uint32_t>(),
      MakeCompositeAttribute<double>(),
      MakeCompositeAttribute<opentelemetry::nostd::string_view>(),
      MakeCompositeAttribute<std::uint64_t>(),
      MakeCompositeAttribute<std::uint8_t>()};

  for (auto const& value : values) {
    v2::Span::Attributes attributes;
    AddAttribute(attributes, "key", value, 32);
    EXPECT_EQ(attributes.dropped_attributes_count(), 1);
    EXPECT_THAT(attributes.attribute_map(), IsEmpty());
  }
}

TEST(Recordable, SetName) {
  auto rec = Recordable(Project(kProjectId));
  rec.SetName("name");
  auto proto = std::move(rec).as_proto();
  EXPECT_EQ(proto.display_name().value(), "name");
}

TEST(Recordable, SetNameTruncates) {
  std::string const name(kDisplayNameStringLimit + 1, 'A');
  std::string const expected(kDisplayNameStringLimit, 'A');

  auto rec = Recordable(Project(kProjectId));
  rec.SetName(name);
  auto proto = std::move(rec).as_proto();
  EXPECT_EQ(proto.display_name().value(), expected);
  EXPECT_EQ(proto.display_name().truncated_byte_count(), 1);
}

TEST(Recordable, SetAttribute) {
  v2::AttributeValue expected;
  expected.mutable_string_value()->set_value("value");

  auto rec = Recordable(Project(kProjectId));
  rec.SetAttribute("key", "value");
  auto proto = std::move(rec).as_proto();
  EXPECT_THAT(proto.attributes().attribute_map(),
              ElementsAre(Pair("key", IsProtoEqual(expected))));
}

TEST(Recordable, SetAttributeRespectsLimit) {
  auto rec = Recordable(Project(kProjectId));
  for (std::size_t i = 0; i != kSpanAttributeLimit + 1; ++i) {
    rec.SetAttribute("key" + std::to_string(i), "value");
  }
  auto proto = std::move(rec).as_proto();
  auto& attributes = *proto.mutable_attributes();
  EXPECT_THAT(attributes.attribute_map(), SizeIs(kSpanAttributeLimit));
  EXPECT_EQ(attributes.dropped_attributes_count(), 1);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
