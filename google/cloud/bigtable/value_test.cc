// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/value.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/type/date.pb.h>
#include <gmock/gmock.h>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Not;

absl::Time MakeTime(std::int64_t sec, int nanos) {
  return absl::FromUnixSeconds(sec) + absl::Nanoseconds(nanos);
}

std::vector<Timestamp> TestTimes() {
  std::vector<Timestamp> times;
  for (auto s : {
           std::int64_t{-9223372035LL},  // near the limit of 64-bit/ns clock
           std::int64_t{-2147483649LL},  // below min 32-bit value
           std::int64_t{-2147483648LL},  // min 32-bit value
           std::int64_t{-1},             // just before Unix epoch
           std::int64_t{0},              // Unix epoch
           std::int64_t{1},              // just after Unix epoch
           std::int64_t{1561147549LL},   // contemporary
           std::int64_t{2147483647LL},   // max 32-bit value
           std::int64_t{2147483648LL},   // above max 32-bit value
           std::int64_t{9223372036LL},   // near the limit of 64-bit/ns clock
       }) {
    for (auto nanos : {-1, 0, 1}) {
      times.push_back(MakeTimestamp(MakeTime(s, nanos)).value());
    }
  }
  return times;
}

template <typename T>
void TestBasicSemantics(T init) {
  Value const default_ctor{};
  EXPECT_THAT(default_ctor.get<T>(), Not(IsOk()));

  Value const v{init};

  ASSERT_STATUS_OK(v.get<T>());
  EXPECT_EQ(init, *v.get<T>());

  Value copy = v;
  EXPECT_EQ(copy, v);
  Value const moved = std::move(copy);
  EXPECT_EQ(moved, v);

  // Tests a null Value of type `T`.
  Value const null = MakeNullValue<T>();

  EXPECT_THAT(null.get<T>(), Not(IsOk()));
  ASSERT_STATUS_OK(null.get<absl::optional<T>>());
  EXPECT_EQ(absl::optional<T>{}, *null.get<absl::optional<T>>());

  Value copy_null = null;
  EXPECT_EQ(copy_null, null);
  Value const moved_null = std::move(copy_null);
  EXPECT_EQ(moved_null, null);

  // Round-trip from Value -> Proto(s) -> Value
  auto const protos = bigtable_internal::ToProto(v);
  EXPECT_EQ(v, bigtable_internal::FromProto(protos.first, protos.second));

  // Ensures that the protos for a NULL T have the same "type" as a non-null T.
  auto const null_protos = bigtable_internal::ToProto(null);
  EXPECT_THAT(null_protos.first, IsProtoEqual(protos.first));

  Value const not_null{absl::optional<T>(init)};
  ASSERT_STATUS_OK(not_null.get<T>());
  EXPECT_EQ(init, *not_null.get<T>());
  ASSERT_STATUS_OK(not_null.get<absl::optional<T>>());
  EXPECT_EQ(init, **not_null.get<absl::optional<T>>());
}

TEST(Value, BasicSemantics) {
  for (auto x : {false, true}) {
    SCOPED_TRACE("Testing: bool " + std::to_string(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<bool>(5, x));
    // std::vector<absl::optional<bool>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }

  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, max64}) {
    SCOPED_TRACE("Testing: std::int64_t " + std::to_string(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<std::int64_t>(5, x));
    // std::vector<absl::optional<std::int64_t>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }

  for (auto x : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
    SCOPED_TRACE("Testing: double " + std::to_string(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<double>(5, x));
    // std::vector<absl::optional<double>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }

  for (auto x : {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F}) {
    SCOPED_TRACE("Testing: float " + std::to_string(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<float>(5, x));
    // std::vector<absl::optional<float>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }

  for (auto const& x :
       std::vector<std::string>{"", "f", "foo", "12345678901234567"}) {
    SCOPED_TRACE("Testing: std::string " + std::string(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<std::string>(5, x));
    // std::vector<absl::optional<std::string>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }
  for (auto const& x : std::vector<Bytes>{Bytes(""), Bytes("f"), Bytes("foo"),
                                          Bytes("12345678901234567")}) {
    SCOPED_TRACE("Testing: Bytes " + x.get<std::string>());
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<Bytes>(5, x));
    // std::vector<absl::optional<Bytes>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }

  for (auto ts : TestTimes()) {
    SCOPED_TRACE("Testing: google::cloud::bigtable::Timestamp " +
                 bigtable_internal::TimestampToRFC3339(ts));
    std::cout << "Testing: google::cloud::bigtable::Timestamp "
              << bigtable_internal::TimestampToRFC3339(ts) << std::endl;
    TestBasicSemantics(ts);
    // uncomment after enabling vector support
    // std::vector<Timestamp> v(5, ts);
    // TestBasicSemantics(v);
    // std::vector<absl::optional<Timestamp>> ov(5, ts);
    // ov.resize(10);
    // TestBasicSemantics(ov);
  }
  for (auto x : {
           absl::CivilDay(1582, 10, 15),  // start of Gregorian calendar
           absl::CivilDay(1677, 9, 21),   // before system_clock limit
           absl::CivilDay(1901, 12, 13),  // around min 32-bit seconds limit
           absl::CivilDay(1970, 1, 1),    // the unix epoch
           absl::CivilDay(2019, 6, 21),   // contemporary
           absl::CivilDay(2038, 1, 19),   // around max 32-bit seconds limit
           absl::CivilDay(2262, 4, 12)    // after system_clock limit
       }) {
    SCOPED_TRACE("Testing: absl::CivilDay " + absl::FormatCivilTime(x));
    TestBasicSemantics(x);
    // uncomment after enabling vector support
    // TestBasicSemantics(std::vector<absl::CivilDay>(5, x));
    // std::vector<absl::optional<absl::CivilDay>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }
}

TEST(Value, Equality) {
  std::vector<std::pair<Value, Value>> test_cases = {
      {Value(false), Value(true)},
      {Value(0), Value(1)},
      {Value(static_cast<std::int64_t>(0)),
       Value(static_cast<std::int64_t>(1))},
      {Value(3.14F), Value(42.0F)},
      {Value(3.14), Value(42.0)},
      {Value("foo"), Value("bar")},
      {Value(Bytes("foo")), Value(Bytes("bar"))},
      {Value(absl::CivilDay(1970, 1, 1)), Value(absl::CivilDay(2020, 3, 15))},
  };

  for (auto const& tc : test_cases) {
    EXPECT_EQ(tc.first, tc.first);
    EXPECT_EQ(tc.second, tc.second);
    EXPECT_NE(tc.first, tc.second);
    // Compares tc.first to tc2.second, which ensures that different "kinds" of
    // value are never equal.
    for (auto const& tc2 : test_cases) {
      EXPECT_NE(tc.first, tc2.second);
    }
  }
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// bigtable::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetString) {
  using Type = std::string;
  Type const data = std::string(128, 'x');
  Value v(data);

  auto s = v.get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ("", *s);
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// bigtable::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetOptionalString) {
  using Type = absl::optional<std::string>;
  Type const data = std::string(128, 'x');
  Value v(data);

  auto s = v.get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(*data, **s);

  s = std::move(v).get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(*data, **s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ("", **s);
}

TEST(Value, BytesRelationalOperators) {
  Bytes b1(std::string(1, '\x00'));
  Bytes b2(std::string(1, '\xff'));

  EXPECT_EQ(b1, b1);
  EXPECT_NE(b1, b2);
}

TEST(Value, ConstructionFromLiterals) {
  Value v_bool(true);
  EXPECT_EQ(true, *v_bool.get<bool>());

  Value v_int64(42);
  EXPECT_EQ(42, *v_int64.get<std::int64_t>());

  Value v_float32(1.5F);
  EXPECT_EQ(1.5F, *v_float32.get<float>());

  Value v_float64(1.5);
  EXPECT_EQ(1.5, *v_float64.get<double>());

  Value v_string("hello");
  EXPECT_EQ("hello", *v_string.get<std::string>());
}

TEST(Value, MixingTypes) {
  using A = bool;
  using B = std::int64_t;

  Value a(A{});
  EXPECT_STATUS_OK(a.get<A>());
  EXPECT_THAT(a.get<B>(), Not(IsOk()));

  Value null_a = MakeNullValue<A>();
  EXPECT_THAT(null_a.get<A>(), Not(IsOk()));
  EXPECT_THAT(null_a.get<B>(), Not(IsOk()));

  EXPECT_NE(null_a, a);

  Value b(B{});
  EXPECT_STATUS_OK(b.get<B>());
  EXPECT_THAT(b.get<A>(), Not(IsOk()));

  EXPECT_NE(b, a);
  EXPECT_NE(b, null_a);

  Value null_b = MakeNullValue<B>();
  EXPECT_THAT(null_b.get<B>(), Not(IsOk()));
  EXPECT_THAT(null_b.get<A>(), Not(IsOk()));

  EXPECT_NE(null_b, b);
  EXPECT_NE(null_b, null_a);
  EXPECT_NE(null_b, a);
}

TEST(Value, ProtoConversionBool) {
  for (auto b : {true, false}) {
    Value const v(b);
    auto const p = bigtable_internal::ToProto(Value(b));
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_THAT(p.first.has_bool_type(), true);
    EXPECT_EQ(b, p.second.bool_value());
  }
}

TEST(Value, ProtoConversionInt64) {
  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, 42, max64}) {
    Value const v(x);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.first.has_int64_type());
    EXPECT_TRUE(p.second.has_int_value());
    EXPECT_EQ(x, p.second.int_value());
  }
}

TEST(Value, ProtoConversionFloat64) {
  for (auto x : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
    Value const v(x);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.first.has_float64_type());
    EXPECT_TRUE(p.second.has_float_value());
    EXPECT_EQ(x, p.second.float_value());
  }

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Tests special cases
  auto const infval = std::numeric_limits<double>::infinity();
  auto const nanval = std::nan("NaN");
  EXPECT_ANY_THROW(auto const x = Value(infval));
  EXPECT_ANY_THROW(auto const x = Value(-infval));
  EXPECT_ANY_THROW(auto const x = Value(nanval));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(Value, ProtoConversionFloat32) {
  for (auto x : {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F}) {
    Value const v(x);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.first.has_float32_type());
    EXPECT_TRUE(p.second.has_float_value());
    EXPECT_EQ(x, p.second.float_value());
  }

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Tests special cases
  auto const nanval = std::nanf("NaN");
  auto const infval = std::numeric_limits<float>::infinity();
  EXPECT_ANY_THROW(auto const x = Value(infval));
  EXPECT_ANY_THROW(auto const x = Value(-infval));
  EXPECT_ANY_THROW(auto const x = Value(nanval));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(Value, ProtoConversionString) {
  for (auto const& x :
       std::vector<std::string>{"", "f", "foo", "12345678901234567890"}) {
    Value const v(x);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.first.has_string_type());
    EXPECT_TRUE(p.second.has_string_value());
    EXPECT_EQ(x, p.second.string_value());
  }
}

TEST(Value, ProtoConversionTimestamp) {
  for (auto ts : TestTimes()) {
    Value const v(ts);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.second.has_timestamp_value());
    EXPECT_EQ(bigtable_internal::TimestampToRFC3339(ts),
              bigtable_internal::TimestampToRFC3339(
                  MakeTimestamp(p.second.timestamp_value()).value()));
  }
}

type::Date BuildDate(int year, int month, int day) {
  auto d = type::Date();
  d.set_year(year);
  d.set_month(month);
  d.set_day(day);
  return d;
}

TEST(Value, ProtoConversionDate) {
  struct {
    absl::CivilDay day;
    type::Date expected;
  } test_cases[] = {
      {absl::CivilDay(-9999, 1, 2), BuildDate(-9999, 1, 2)},
      {absl::CivilDay(-999, 1, 2), BuildDate(-999, 1, 2)},
      {absl::CivilDay(-1, 1, 2), BuildDate(-1, 1, 2)},
      {absl::CivilDay(0, 1, 2), BuildDate(0, 1, 2)},
      {absl::CivilDay(1, 1, 2), BuildDate(1, 1, 2)},
      {absl::CivilDay(999, 1, 2), BuildDate(999, 1, 2)},
      {absl::CivilDay(1582, 10, 15), BuildDate(1582, 10, 15)},
      {absl::CivilDay(1677, 9, 21), BuildDate(1677, 9, 21)},
      {absl::CivilDay(1901, 12, 13), BuildDate(1901, 12, 13)},
      {absl::CivilDay(1970, 1, 1), BuildDate(1970, 1, 1)},
      {absl::CivilDay(2019, 6, 21), BuildDate(2019, 6, 21)},
      {absl::CivilDay(2038, 1, 19), BuildDate(2038, 1, 19)},
      {absl::CivilDay(2262, 4, 12), BuildDate(2262, 4, 12)},
  };

  for (auto const& tc : test_cases) {
    SCOPED_TRACE("CivilDay: " + absl::FormatCivilTime(tc.day));
    Value const v(tc.day);
    auto const p = bigtable_internal::ToProto(v);
    EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
    EXPECT_TRUE(p.first.has_date_type());
    EXPECT_EQ(tc.expected.year(), p.second.date_value().year());
    EXPECT_EQ(tc.expected.month(), p.second.date_value().month());
    EXPECT_EQ(tc.expected.day(), p.second.date_value().day());
  }
}

void SetNullProtoKind(Value& v) {
  auto p = bigtable_internal::ToProto(v);
  p.second.clear_kind();
  p.second.clear_type();
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, bool x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_bool_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, double x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_float_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, float x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_float_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, char const* x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_string_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void ClearProtoKind(Value& v) {
  auto p = bigtable_internal::ToProto(v);
  p.first.clear_kind();
  p.second.clear_kind();
  v = bigtable_internal::FromProto(p.first, p.second);
}

TEST(Value, GetBadBool) {
  Value v(true);
  ClearProtoKind(v);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, 0.0F);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, "hello");
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));
}

TEST(Value, GetBadFloat64) {
  Value v(0.0);
  EXPECT_THAT(v.get<double>(), IsOk());

  ClearProtoKind(v);
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetProtoKind(v, "bad string");
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  // We also confirm disallowed values
  SetProtoKind(v, std::numeric_limits<double>::infinity());
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetProtoKind(v, -std::numeric_limits<double>::infinity());
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetProtoKind(v, std::nan("NaN"));
  EXPECT_THAT(v.get<double>(), Not(IsOk()));
}

TEST(Value, GetBadFloat32) {
  Value v(0.0F);
  EXPECT_THAT(v.get<float>(), IsOk());

  ClearProtoKind(v);
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetProtoKind(v, "bad string");
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  // We also confirm disallowed values
  SetProtoKind(v, std::numeric_limits<float>::infinity());
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetProtoKind(v, -std::numeric_limits<float>::infinity());
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetProtoKind(v, std::nan("NaN"));
  EXPECT_THAT(v.get<float>(), Not(IsOk()));
}

TEST(Value, GetBadString) {
  Value v("hello");
  ClearProtoKind(v);
  EXPECT_THAT(v.get<std::string>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<std::string>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<std::string>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<std::string>(), Not(IsOk()));
}

TEST(Value, GetBadBytes) {
  Value v(Bytes("hello"));
  ClearProtoKind(v);
  EXPECT_THAT(v.get<Bytes>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<Bytes>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<Bytes>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<Bytes>(), Not(IsOk()));
}

TEST(Value, GetBadTimestamp) {
  Value v(Timestamp{});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));
}

TEST(Value, GetBadDate) {
  Value v(absl::CivilDay{});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));
}

TEST(Value, GetBadOptional) {
  Value v(absl::optional<double>{});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<absl::optional<double>>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<absl::optional<double>>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<absl::optional<double>>(), Not(IsOk()));
}

TEST(Value, OutputStream) {
  auto const normal = [](std::ostream& os) -> std::ostream& { return os; };
  auto const hex = [](std::ostream& os) -> std::ostream& {
    return os << std::hex;
  };
  auto const boolalpha = [](std::ostream& os) -> std::ostream& {
    return os << std::boolalpha;
  };
  auto const float4 = [](std::ostream& os) -> std::ostream& {
    return os << std::showpoint << std::setprecision(4);
  };

  struct TestCase {
    Value value;
    std::string expected;
    std::function<std::ostream&(std::ostream&)> manip;
  };

  std::vector<TestCase> test_case = {
      {Value(false), "0", normal},
      {Value(true), "1", normal},
      {Value(false), "false", boolalpha},
      {Value(true), "true", boolalpha},
      {Value(42), "42", normal},
      {Value(42), "2a", hex},
      {Value(42.0), "42", normal},
      {Value(42.0), "42.00", float4},
      {Value(42.0F), "42", normal},
      {Value(42.0F), "42.00", float4},
      {Value(""), "", normal},
      {Value("foo"), "foo", normal},
      {Value("NULL"), "NULL", normal},
      {Value(Bytes(std::string("DEADBEEF"))), R"(B"DEADBEEF")", normal},
      {Value(Timestamp()), "1970-01-01T00:00:00Z", normal},
      {Value(absl::CivilDay()), "1970-01-01", normal},

      // Tests string quoting: No quotes for scalars; quotes within aggregates
      {Value(""), "", normal},
      {Value("foo"), "foo", normal},

      // Tests null values
      {MakeNullValue<bool>(), "NULL", normal},
      {MakeNullValue<std::int64_t>(), "NULL", normal},
      {MakeNullValue<double>(), "NULL", normal},
      {MakeNullValue<float>(), "NULL", normal},
      {MakeNullValue<std::string>(), "NULL", normal},
      {MakeNullValue<Bytes>(), "NULL", normal},
      {MakeNullValue<Timestamp>(), "NULL", normal},
      {MakeNullValue<absl::CivilDay>(), "NULL", normal},

  };

  for (auto const& tc : test_case) {
    std::stringstream ss;
    tc.manip(ss) << tc.value;
    EXPECT_EQ(ss.str(), tc.expected);
  }
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
