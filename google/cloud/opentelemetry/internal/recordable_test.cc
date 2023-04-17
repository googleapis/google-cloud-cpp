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
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/version.h"
#include "absl/time/clock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::devtools::cloudtrace::v2;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::SizeIs;

auto constexpr kProjectId = "test-project";

template <typename T>
opentelemetry::nostd::span<T> MakeCompositeAttribute() {
  T v[]{T{}, T{}};
  return opentelemetry::nostd::span<T>(v);
}

Matcher<v2::AttributeValue const&> AttributeValue(bool value) {
  return ResultOf(
      "bool_value",
      [](v2::AttributeValue const& av) { return av.bool_value(); }, Eq(value));
}

Matcher<v2::AttributeValue const&> AttributeValue(int value) {
  return ResultOf(
      "int_value", [](v2::AttributeValue const& av) { return av.int_value(); },
      Eq(value));
}

Matcher<v2::AttributeValue const&> AttributeValue(
    std::string const& value, int truncated_byte_count = 0) {
  return AllOf(ResultOf(
                   "string_value",
                   [](v2::AttributeValue const& av) {
                     return av.string_value().value();
                   },
                   Eq(value)),
               ResultOf(
                   "truncated_byte_count",
                   [](v2::AttributeValue const& av) {
                     return av.string_value().truncated_byte_count();
                   },
                   Eq(truncated_byte_count)));
}

Matcher<v2::AttributeValue const&> AttributeValue(
    char const* value, int truncated_byte_count = 0) {
  return AttributeValue(std::string{value}, truncated_byte_count);
}

Matcher<v2::Span::Attributes const&> Attributes(
    Matcher<protobuf::Map<std::string, v2::AttributeValue> const&> const&
        attribute_map_matcher,
    int dropped_attributes_count = 0) {
  return AllOf(
      ResultOf(
          "attribute_map",
          [](v2::Span::Attributes const& a) { return a.attribute_map(); },
          attribute_map_matcher),
      ResultOf(
          "dropped_attributes_count",
          [](v2::Span::Attributes const& a) {
            return a.dropped_attributes_count();
          },
          Eq(dropped_attributes_count)));
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
  AddAttribute(attributes, "accepted", "value", /*limit=*/1);
  EXPECT_THAT(attributes, Attributes(ElementsAre(Pair("accepted", _))));

  AddAttribute(attributes, "rejected", "value", /*limit=*/1);
  EXPECT_THAT(attributes, Attributes(ElementsAre(Pair("accepted", _)),
                                     /*dropped_attributes_count=*/1));
}

TEST(AddAttribute, AcceptsExistingKeyAtLimit) {
  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", "value1", /*limit=*/1);
  EXPECT_THAT(attributes,
              Attributes(ElementsAre(Pair("key", AttributeValue("value1")))));

  // The map is full, but we should still be able to overwrite an existing key.
  AddAttribute(attributes, "key", "value2", /*limit=*/1);
  EXPECT_THAT(attributes,
              Attributes(ElementsAre(Pair("key", AttributeValue("value2"))),
                         /*dropped_attributes_count=*/0));
}

TEST(AddAttribute, DropsLongKey) {
  std::string const long_key(kAttributeKeyStringLimit + 1, 'A');

  v2::Span::Attributes attributes;
  AddAttribute(attributes, long_key, "value", /*limit=*/32);
  EXPECT_THAT(attributes,
              Attributes(IsEmpty(), /*dropped_attributes_count=*/1));
}

TEST(AddAttribute, HandlesBoolAttribute) {
  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", true, /*limit=*/32);
  EXPECT_THAT(attributes,
              Attributes(ElementsAre(Pair("key", AttributeValue(true)))));
}

TEST(AddAttribute, HandlesIntAttributes) {
  std::vector<opentelemetry::common::AttributeValue> values = {
      std::int32_t{42}, std::uint32_t{42}, std::int64_t{42}, std::uint64_t{42}};

  for (auto const& value : values) {
    v2::Span::Attributes attributes;
    AddAttribute(attributes, "key", value, /*limit=*/32);
    EXPECT_THAT(attributes,
                Attributes(ElementsAre(Pair("key", AttributeValue(42)))));
  }
}

TEST(AddAttribute, HandlesStringAttributes) {
  std::vector<opentelemetry::common::AttributeValue> values = {
      "value", opentelemetry::nostd::string_view{"value"}};

  for (auto const& value : values) {
    v2::Span::Attributes attributes;
    AddAttribute(attributes, "key", value, /*limit=*/32);
    EXPECT_THAT(attributes,
                Attributes(ElementsAre(Pair("key", AttributeValue("value")))));
  }
}

TEST(AddAttribute, TruncatesStringValue) {
  std::string const long_value(kAttributeValueStringLimit + 1, 'A');
  std::string const expected_value(kAttributeValueStringLimit, 'A');

  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", long_value, /*limit=*/32);
  EXPECT_THAT(
      attributes,
      Attributes(ElementsAre(Pair(
          "key", AttributeValue(expected_value, /*truncated_byte_count=*/1)))));
}

TEST(AddAttribute, ConvertsDoubleAttributeToString) {
  v2::Span::Attributes attributes;
  AddAttribute(attributes, "key", 4.2, /*limit=*/32);
  EXPECT_THAT(attributes,
              Attributes(ElementsAre(Pair("key", AttributeValue("4.2")))));
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
    AddAttribute(attributes, "key", value, /*limit=*/32);
    EXPECT_THAT(attributes,
                Attributes(IsEmpty(), /*dropped_attributes_count=*/1));
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

TEST(Recordable, SetIdentity) {
  opentelemetry::trace::TraceId const trace_id(
      std::array<uint8_t const, opentelemetry::trace::TraceId::kSize>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));

  opentelemetry::trace::SpanId const span_id(
      std::array<uint8_t const, opentelemetry::trace::SpanId::kSize>(
          {1, 1, 2, 2, 3, 3, 4, 4}));

  opentelemetry::trace::SpanId const parent_span_id(
      std::array<uint8_t const, opentelemetry::trace::SpanId::kSize>(
          {5, 5, 6, 6, 7, 7, 8, 8}));

  opentelemetry::trace::SpanContext span_context(trace_id, span_id, {}, false);

  auto rec = Recordable(Project(kProjectId));
  rec.SetIdentity(span_context, parent_span_id);
  auto proto = std::move(rec).as_proto();

  EXPECT_EQ(proto.name(),
            "projects/test-project/traces/000102030405060708090a0b0c0d0e0f/"
            "spans/0101020203030404");
  EXPECT_EQ(proto.span_id(), "0101020203030404");
  EXPECT_EQ(proto.parent_span_id(), "0505060607070808");
}

TEST(Recordable, SetAttribute) {
  auto rec = Recordable(Project(kProjectId));
  rec.SetAttribute("key", "value");
  auto proto = std::move(rec).as_proto();
  EXPECT_THAT(proto.attributes(),
              Attributes(ElementsAre(Pair("key", AttributeValue("value")))));
}

TEST(Recordable, SetAttributeRespectsLimit) {
  auto rec = Recordable(Project(kProjectId));
  for (std::size_t i = 0; i != kSpanAttributeLimit + 1; ++i) {
    rec.SetAttribute("key" + std::to_string(i), "value");
  }
  auto proto = std::move(rec).as_proto();
  EXPECT_THAT(proto.attributes(), Attributes(SizeIs(kSpanAttributeLimit),
                                             /*dropped_attributes_count=*/1));
}

TEST(Recordable, SetStartTime) {
  auto start = std::chrono::system_clock::now();
  auto expected = absl::FromChrono(start);

  auto rec = Recordable(Project(kProjectId));
  rec.SetStartTime(start);
  auto proto = std::move(rec).as_proto();
  auto actual = internal::ToAbslTime(proto.start_time());
  EXPECT_EQ(actual, expected);
}

TEST(Recordable, SetDuration) {
  auto start = std::chrono::system_clock::now();
  auto duration = std::chrono::nanoseconds(12345);
  auto expected = absl::FromChrono(start) + absl::FromChrono(duration);

  auto rec = Recordable(Project(kProjectId));
  rec.SetStartTime(start);
  rec.SetDuration(duration);
  auto proto = std::move(rec).as_proto();
  auto actual = internal::ToAbslTime(proto.end_time());
  EXPECT_EQ(actual, expected);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
