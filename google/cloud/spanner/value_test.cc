// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/value.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <chrono>
#include <cmath>
#include <ios>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
MakeTimePoint(std::time_t sec, std::chrono::nanoseconds::rep nanos) {
  return std::chrono::system_clock::from_time_t(sec) +
         std::chrono::nanoseconds(nanos);
}

template <typename T>
void TestBasicSemantics(T init) {
  Value const default_ctor{};
  EXPECT_FALSE(default_ctor.get<T>().ok());

  Value const v{init};

  EXPECT_STATUS_OK(v.get<T>());
  EXPECT_EQ(init, *v.get<T>());

  Value copy = v;
  EXPECT_EQ(copy, v);
  Value const moved = std::move(copy);
  EXPECT_EQ(moved, v);

  // Tests a null Value of type `T`.
  Value const null = MakeNullValue<T>();

  EXPECT_FALSE(null.get<T>().ok());
  EXPECT_STATUS_OK(null.get<absl::optional<T>>());
  EXPECT_EQ(absl::optional<T>{}, *null.get<absl::optional<T>>());

  Value copy_null = null;
  EXPECT_EQ(copy_null, null);
  Value const moved_null = std::move(copy_null);
  EXPECT_EQ(moved_null, null);

  // Round-trip from Value -> Proto(s) -> Value
  auto const protos = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(protos.first, protos.second));

  // Ensures that the protos for a NULL T have the same "type" as a non-null T.
  auto const null_protos = internal::ToProto(null);
  EXPECT_THAT(null_protos.first, IsProtoEqual(protos.first));
  EXPECT_EQ(null_protos.second.null_value(),
            google::protobuf::NullValue::NULL_VALUE);

  Value const not_null{absl::optional<T>(init)};
  EXPECT_STATUS_OK(not_null.get<T>());
  EXPECT_EQ(init, *not_null.get<T>());
  EXPECT_STATUS_OK(not_null.get<absl::optional<T>>());
  EXPECT_EQ(init, **not_null.get<absl::optional<T>>());
}

TEST(Value, BasicSemantics) {
  for (auto x : {false, true}) {
    SCOPED_TRACE("Testing: bool " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<bool>(5, x));
    std::vector<absl::optional<bool>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, max64}) {
    SCOPED_TRACE("Testing: std::int64_t " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<std::int64_t>(5, x));
    std::vector<absl::optional<std::int64_t>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  // Note: We skip testing the NaN case here because NaN always compares not
  // equal, even with itself. So NaN is handled in a separate test.
  auto const inf = std::numeric_limits<double>::infinity();
  for (auto x : {-inf, -1.0, -0.5, 0.0, 0.5, 1.0, inf}) {
    SCOPED_TRACE("Testing: double " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<double>(5, x));
    std::vector<absl::optional<double>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (auto const& x :
       std::vector<std::string>{"", "f", "foo", "12345678901234567"}) {
    SCOPED_TRACE("Testing: std::string " + std::string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<std::string>(5, x));
    std::vector<absl::optional<std::string>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (auto const& x : std::vector<Bytes>{Bytes(""), Bytes("f"), Bytes("foo"),
                                          Bytes("12345678901234567")}) {
    SCOPED_TRACE("Testing: Bytes " + x.get<std::string>());
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<Bytes>(5, x));
    std::vector<absl::optional<Bytes>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (auto const& x : {
           MakeNumeric(-0.9e29).value(),
           MakeNumeric(-1).value(),
           MakeNumeric(-1.0e-9).value(),
           Numeric(),
           MakeNumeric(1.0e-9).value(),
           MakeNumeric(1U).value(),
           MakeNumeric(0.9e29).value(),
       }) {
    SCOPED_TRACE("Testing: google::cloud::spanner::Numeric " + x.ToString());
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<Numeric>(5, x));
    std::vector<absl::optional<Numeric>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (time_t t : {
           -9223372035LL,   // near the limit of 64-bit/ns system_clock
           -2147483649LL,   // below min 32-bit int
           -2147483648LL,   // min 32-bit int
           -1LL, 0LL, 1LL,  // around the unix epoch
           1561147549LL,    // contemporary
           2147483647LL,    // max 32-bit int
           2147483648LL,    // above max 32-bit int
           9223372036LL     // near the limit of 64-bit/ns system_clock
       }) {
    for (auto nanos : {-1, 0, 1}) {
      auto ts = MakeTimestamp(MakeTimePoint(t, nanos)).value();
      SCOPED_TRACE("Testing: google::cloud::spanner::Timestamp " +
                   internal::TimestampToRFC3339(ts));
      TestBasicSemantics(ts);
      std::vector<Timestamp> v(5, ts);
      TestBasicSemantics(v);
      std::vector<absl::optional<Timestamp>> ov(5, ts);
      ov.resize(10);
      TestBasicSemantics(ov);
    }
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
    TestBasicSemantics(std::vector<absl::CivilDay>(5, x));
    std::vector<absl::optional<absl::CivilDay>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  // No for-loop because there's only one value of CommitTimestamp
  auto const x = CommitTimestamp{};
  TestBasicSemantics(x);
  TestBasicSemantics(std::vector<CommitTimestamp>(5, x));
  std::vector<absl::optional<CommitTimestamp>> v(5, x);
  v.resize(10);
  TestBasicSemantics(v);
}

TEST(Value, Equality) {
  std::vector<std::pair<Value, Value>> test_cases = {
      {Value(false), Value(true)},
      {Value(0), Value(1)},
      {Value(3.14), Value(42.0)},
      {Value("foo"), Value("bar")},
      {Value(Bytes("foo")), Value(Bytes("bar"))},
      {Value(MakeNumeric(0).value()), Value(MakeNumeric(1).value())},
      {Value(absl::CivilDay(1970, 1, 1)), Value(absl::CivilDay(2020, 3, 15))},
      {Value(std::vector<double>{1.2, 3.4}),
       Value(std::vector<double>{4.5, 6.7})},
      {Value(std::make_tuple(false, 123, "foo")),
       Value(std::make_tuple(true, 456, "bar"))},
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
// spanner::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetString) {
  using Type = std::string;
  Type const data = std::string(128, 'x');
  Value v(data);

  auto s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ("", *s);
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// spanner::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetOptionalString) {
  using Type = absl::optional<std::string>;
  Type const data = std::string(128, 'x');
  Value v(data);

  auto s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(*data, **s);

  s = std::move(v).get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(*data, **s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ("", **s);
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// spanner::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetVectorString) {
  using Type = std::vector<std::string>;
  Type const data(128, std::string(128, 'x'));
  Value v(data);

  auto s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(Type(data.size(), ""), *s);
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// spanner::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetStructString) {
  using Type = std::tuple<std::pair<std::string, std::string>, std::string>;
  Type data{std::make_pair("name", std::string(128, 'x')),
            std::string(128, 'x')};
  Value v(data);

  auto s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = v.get<Type>();
  EXPECT_STATUS_OK(s);
  EXPECT_EQ(Type({"name", ""}, ""), *s);
}

TEST(Value, DoubleNaN) {
  double const nan = std::nan("NaN");
  Value v{nan};
  EXPECT_TRUE(std::isnan(*v.get<double>()));

  // Since IEEE 754 defines that nan is not equal to itself, then a Value with
  // NaN should not be equal to itself.
  EXPECT_NE(nan, nan);
  EXPECT_NE(v, v);
}

TEST(Value, BytesDecodingError) {
  Value const v(Bytes("some data"));
  auto p = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(p.first, p.second));

  // Now we corrupt the Value proto so that it cannot be decoded.
  p.second.set_string_value("not base64 encoded data");
  Value bad = internal::FromProto(p.first, p.second);
  EXPECT_NE(v, bad);

  // We know the type is Bytes, but we cannot get a value out of it because the
  // base64 decoding will fail.
  StatusOr<Bytes> bytes = bad.get<Bytes>();
  EXPECT_THAT(bytes,
              StatusIs(Not(StatusCode::kOk), HasSubstr("Invalid base64")));
}

TEST(Value, BytesRelationalOperators) {
  Bytes b1(std::string(1, '\x00'));
  Bytes b2(std::string(1, '\xff'));

  EXPECT_EQ(b1, b1);
  EXPECT_NE(b1, b2);
}

TEST(Value, ConstructionFromLiterals) {
  Value v_int64(42);
  EXPECT_EQ(42, *v_int64.get<std::int64_t>());

  Value v_string("hello");
  EXPECT_EQ("hello", *v_string.get<std::string>());

  std::vector<char const*> vec = {"foo", "bar"};
  Value v_vec(vec);
  EXPECT_STATUS_OK(v_vec.get<std::vector<std::string>>());

  std::tuple<char const*, char const*> tup = std::make_tuple("foo", "bar");
  Value v_tup(tup);
  EXPECT_STATUS_OK((v_tup.get<std::tuple<std::string, std::string>>()));

  auto named_field = std::make_tuple(false, std::make_pair("f1", 42));
  Value v_named_field(named_field);
  EXPECT_STATUS_OK(
      (v_named_field
           .get<std::tuple<bool, std::pair<std::string, std::int64_t>>>()));
}

TEST(Value, MixingTypes) {
  using A = bool;
  using B = std::int64_t;

  Value a(A{});
  EXPECT_STATUS_OK(a.get<A>());
  EXPECT_FALSE(a.get<B>().ok());
  EXPECT_FALSE(a.get<B>().ok());

  Value null_a = MakeNullValue<A>();
  EXPECT_FALSE(null_a.get<A>().ok());
  EXPECT_FALSE(null_a.get<B>().ok());

  EXPECT_NE(null_a, a);

  Value b(B{});
  EXPECT_STATUS_OK(b.get<B>());
  EXPECT_FALSE(b.get<A>().ok());
  EXPECT_FALSE(b.get<A>().ok());

  EXPECT_NE(b, a);
  EXPECT_NE(b, null_a);

  Value null_b = MakeNullValue<B>();
  EXPECT_FALSE(null_b.get<B>().ok());
  EXPECT_FALSE(null_b.get<A>().ok());

  EXPECT_NE(null_b, b);
  EXPECT_NE(null_b, null_a);
  EXPECT_NE(null_b, a);
}

TEST(Value, SpannerArray) {
  using ArrayInt64 = std::vector<std::int64_t>;
  using ArrayDouble = std::vector<double>;

  ArrayInt64 const empty = {};
  Value const ve(empty);
  EXPECT_EQ(ve, ve);
  EXPECT_STATUS_OK(ve.get<ArrayInt64>());
  EXPECT_FALSE(ve.get<ArrayDouble>().ok());
  EXPECT_EQ(empty, *ve.get<ArrayInt64>());

  ArrayInt64 const ai = {1, 2, 3};
  Value const vi(ai);
  EXPECT_EQ(vi, vi);
  EXPECT_STATUS_OK(vi.get<ArrayInt64>());
  EXPECT_FALSE(vi.get<ArrayDouble>().ok());
  EXPECT_EQ(ai, *vi.get<ArrayInt64>());

  ArrayDouble const ad = {1.0, 2.0, 3.0};
  Value const vd(ad);
  EXPECT_EQ(vd, vd);
  EXPECT_NE(vi, vd);
  EXPECT_FALSE(vd.get<ArrayInt64>().ok());
  EXPECT_STATUS_OK(vd.get<ArrayDouble>());
  EXPECT_EQ(ad, *vd.get<ArrayDouble>());

  Value const null_vi = MakeNullValue<ArrayInt64>();
  EXPECT_EQ(null_vi, null_vi);
  EXPECT_NE(null_vi, vi);
  EXPECT_NE(null_vi, vd);
  EXPECT_FALSE(null_vi.get<ArrayInt64>().ok());
  EXPECT_FALSE(null_vi.get<ArrayDouble>().ok());

  Value const null_vd = MakeNullValue<ArrayDouble>();
  EXPECT_EQ(null_vd, null_vd);
  EXPECT_NE(null_vd, null_vi);
  EXPECT_NE(null_vd, vd);
  EXPECT_NE(null_vd, vi);
  EXPECT_FALSE(null_vd.get<ArrayDouble>().ok());
  EXPECT_FALSE(null_vd.get<ArrayInt64>().ok());
}

TEST(Value, SpannerStruct) {
  // Using declarations to shorten the tests, making them more readable.
  using std::int64_t;
  using std::make_pair;
  using std::make_tuple;
  using std::pair;
  using std::string;
  using std::tuple;

  auto tup1 = make_tuple(false, int64_t{123});
  using T1 = decltype(tup1);
  Value v1(tup1);
  EXPECT_STATUS_OK(v1.get<T1>());
  EXPECT_EQ(tup1, *v1.get<T1>());
  EXPECT_EQ(v1, v1);

  // Verify we can extract tuple elements even if they're wrapped in a pair.
  auto const pair0 = v1.get<tuple<pair<string, bool>, int64_t>>();
  EXPECT_STATUS_OK(pair0);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair0).second);
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair0));
  auto const pair1 = v1.get<tuple<bool, pair<string, int64_t>>>();
  EXPECT_STATUS_OK(pair1);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair1));
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair1).second);
  auto const pair01 =
      v1.get<tuple<pair<string, bool>, pair<string, int64_t>>>();
  EXPECT_STATUS_OK(pair01);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair01).second);
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair01).second);

  auto tup2 = make_tuple(false, make_pair(string("f2"), int64_t{123}));
  using T2 = decltype(tup2);
  Value v2(tup2);
  EXPECT_STATUS_OK(v2.get<T2>());
  EXPECT_EQ(tup2, *v2.get<T2>());
  EXPECT_EQ(v2, v2);
  EXPECT_NE(v2, v1);

  // T1 is lacking field names, but otherwise the same as T2.
  EXPECT_EQ(tup1, *v2.get<T1>());
  EXPECT_NE(tup2, *v1.get<T2>());

  auto tup3 = make_tuple(false, make_pair(string("Other"), int64_t{123}));
  using T3 = decltype(tup3);
  Value v3(tup3);
  EXPECT_STATUS_OK(v3.get<T3>());
  EXPECT_EQ(tup3, *v3.get<T3>());
  EXPECT_EQ(v3, v3);
  EXPECT_NE(v3, v2);
  EXPECT_NE(v3, v1);

  static_assert(std::is_same<T2, T3>::value, "Only diff is field name");

  // v1 != v2, yet T2 works with v1 and vice versa
  EXPECT_NE(v1, v2);
  EXPECT_STATUS_OK(v1.get<T2>());
  EXPECT_STATUS_OK(v2.get<T1>());

  Value v_null(absl::optional<T1>{});
  EXPECT_FALSE(v_null.get<absl::optional<T1>>()->has_value());
  EXPECT_FALSE(v_null.get<absl::optional<T2>>()->has_value());

  EXPECT_NE(v1, v_null);
  EXPECT_NE(v2, v_null);

  auto array_struct = std::vector<T3>{
      T3{false, {"age", 1}},
      T3{true, {"age", 2}},
      T3{false, {"age", 3}},
  };
  using T4 = decltype(array_struct);
  Value v4(array_struct);
  EXPECT_STATUS_OK(v4.get<T4>());
  EXPECT_FALSE(v4.get<T3>().ok());
  EXPECT_FALSE(v4.get<T2>().ok());
  EXPECT_FALSE(v4.get<T1>().ok());

  EXPECT_STATUS_OK(v4.get<T4>());
  EXPECT_EQ(array_struct, *v4.get<T4>());

  auto empty = tuple<>{};
  using T5 = decltype(empty);
  Value v5(empty);
  EXPECT_STATUS_OK(v5.get<T5>());
  EXPECT_FALSE(v5.get<T4>().ok());
  EXPECT_EQ(v5, v5);
  EXPECT_NE(v5, v4);

  EXPECT_STATUS_OK(v5.get<T5>());
  EXPECT_EQ(empty, *v5.get<T5>());

  auto crazy = tuple<tuple<std::vector<absl::optional<bool>>>>{};
  using T6 = decltype(crazy);
  Value v6(crazy);
  EXPECT_STATUS_OK(v6.get<T6>());
  EXPECT_FALSE(v6.get<T5>().ok());
  EXPECT_EQ(v6, v6);
  EXPECT_NE(v6, v5);

  EXPECT_STATUS_OK(v6.get<T6>());
  EXPECT_EQ(crazy, *v6.get<T6>());
}

TEST(Value, SpannerStructWithNull) {
  auto v1 = Value(std::make_tuple(123, true));
  auto v2 = Value(std::make_tuple(123, absl::optional<bool>{}));

  auto protos1 = internal::ToProto(v1);
  auto protos2 = internal::ToProto(v2);

  // The type protos match for both values, but the value protos DO NOT match.
  EXPECT_THAT(protos1.first, IsProtoEqual(protos2.first));
  EXPECT_THAT(protos1.second, Not(IsProtoEqual(protos2.second)));

  // Now verify that the second value has two fields and the second field
  // contains a NULL value.
  ASSERT_EQ(protos2.second.list_value().values_size(), 2);
  ASSERT_EQ(protos2.second.list_value().values(1).null_value(),
            google::protobuf::NullValue::NULL_VALUE);
}

TEST(Value, ProtoConversionBool) {
  for (auto b : {true, false}) {
    Value const v(b);
    auto const p = internal::ToProto(Value(b));
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::BOOL, p.first.code());
    EXPECT_EQ(b, p.second.bool_value());
  }
}

TEST(Value, ProtoConversionInt64) {
  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, 42, max64}) {
    Value const v(x);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::INT64, p.first.code());
    EXPECT_EQ(std::to_string(x), p.second.string_value());
  }
}

TEST(Value, ProtoConversionFloat64) {
  for (auto x : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
    Value const v(x);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, p.first.code());
    EXPECT_EQ(x, p.second.number_value());
  }

  // Tests special cases
  auto const inf = std::numeric_limits<double>::infinity();
  Value v(inf);
  auto p = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(p.first, p.second));
  EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, p.first.code());
  EXPECT_EQ("Infinity", p.second.string_value());

  v = Value(-inf);
  p = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(p.first, p.second));
  EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, p.first.code());
  EXPECT_EQ("-Infinity", p.second.string_value());

  auto const nan = std::nan("NaN");
  v = Value(nan);
  p = internal::ToProto(v);
  // Note: NaN is defined to be not equal to everything, including itself, so
  // we instead ensure that it is not equal with EXPECT_NE.
  EXPECT_NE(v, internal::FromProto(p.first, p.second));
  EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, p.first.code());
  EXPECT_EQ("NaN", p.second.string_value());
}

TEST(Value, ProtoConversionString) {
  for (auto const& x :
       std::vector<std::string>{"", "f", "foo", "12345678901234567890"}) {
    Value const v(x);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::STRING, p.first.code());
    EXPECT_EQ(x, p.second.string_value());
  }
}

TEST(Value, ProtoConversionBytes) {
  for (auto const& x : std::vector<Bytes>{Bytes(""), Bytes("f"), Bytes("foo"),
                                          Bytes("12345678901234567890")}) {
    Value const v(x);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::BYTES, p.first.code());
    EXPECT_EQ(internal::BytesToBase64(x), p.second.string_value());
  }
}

TEST(Value, ProtoConversionNumeric) {
  for (auto const& x : std::vector<Numeric>{
           MakeNumeric(-0.9e29).value(),
           MakeNumeric(-1).value(),
           MakeNumeric(-1.0e-9).value(),
           Numeric(),
           MakeNumeric(1.0e-9).value(),
           MakeNumeric(1U).value(),
           MakeNumeric(0.9e29).value(),
       }) {
    Value const v(x);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::NUMERIC, p.first.code());
    EXPECT_EQ(x.ToString(), p.second.string_value());
  }
}

TEST(Value, ProtoConversionTimestamp) {
  for (time_t t : {
           -9223372035LL,   // near the limit of 64-bit/ns system_clock
           -2147483649LL,   // below min 32-bit int
           -2147483648LL,   // min 32-bit int
           -1LL, 0LL, 1LL,  // around the unix epoch
           1561147549LL,    // contemporary
           2147483647LL,    // max 32-bit int
           2147483648LL,    // above max 32-bit int
           9223372036LL     // near the limit of 64-bit/ns system_clock
       }) {
    for (auto nanos : {-1, 0, 1}) {
      auto ts = MakeTimestamp(MakeTimePoint(t, nanos)).value();
      Value const v(ts);
      auto const p = internal::ToProto(v);
      EXPECT_EQ(v, internal::FromProto(p.first, p.second));
      EXPECT_EQ(google::spanner::v1::TypeCode::TIMESTAMP, p.first.code());
      EXPECT_EQ(internal::TimestampToRFC3339(ts), p.second.string_value());
    }
  }
}

TEST(Value, ProtoConversionDate) {
  struct {
    absl::CivilDay day;
    std::string expected;
  } test_cases[] = {
      {absl::CivilDay(-9999, 1, 2), "-9999-01-02"},
      {absl::CivilDay(-999, 1, 2), "-999-01-02"},
      {absl::CivilDay(-1, 1, 2), "-001-01-02"},
      {absl::CivilDay(0, 1, 2), "0000-01-02"},
      {absl::CivilDay(1, 1, 2), "0001-01-02"},
      {absl::CivilDay(999, 1, 2), "0999-01-02"},
      {absl::CivilDay(1582, 10, 15), "1582-10-15"},
      {absl::CivilDay(1677, 9, 21), "1677-09-21"},
      {absl::CivilDay(1901, 12, 13), "1901-12-13"},
      {absl::CivilDay(1970, 1, 1), "1970-01-01"},
      {absl::CivilDay(2019, 6, 21), "2019-06-21"},
      {absl::CivilDay(2038, 1, 19), "2038-01-19"},
      {absl::CivilDay(2262, 4, 12), "2262-04-12"},
  };

  for (auto const& tc : test_cases) {
    SCOPED_TRACE("CivilDay: " + absl::FormatCivilTime(tc.day));
    Value const v(tc.day);
    auto const p = internal::ToProto(v);
    EXPECT_EQ(v, internal::FromProto(p.first, p.second));
    EXPECT_EQ(google::spanner::v1::TypeCode::DATE, p.first.code());
    EXPECT_EQ(tc.expected, p.second.string_value());
  }
}

TEST(Value, ProtoConversionArray) {
  std::vector<std::int64_t> data{1, 2, 3};
  Value const v(data);
  auto const p = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(p.first, p.second));
  EXPECT_EQ(google::spanner::v1::TypeCode::ARRAY, p.first.code());
  EXPECT_EQ(google::spanner::v1::TypeCode::INT64,
            p.first.array_element_type().code());
  EXPECT_EQ("1", p.second.list_value().values(0).string_value());
  EXPECT_EQ("2", p.second.list_value().values(1).string_value());
  EXPECT_EQ("3", p.second.list_value().values(2).string_value());
}

TEST(Value, ProtoConversionStruct) {
  auto data = std::make_tuple(3.14, std::make_pair("foo", 42));
  Value const v(data);
  auto const p = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(p.first, p.second));
  EXPECT_EQ(google::spanner::v1::TypeCode::STRUCT, p.first.code());

  Value const null_struct_value(
      MakeNullValue<std::tuple<bool, std::int64_t>>());
  auto const null_struct_proto = internal::ToProto(null_struct_value);
  EXPECT_EQ(google::spanner::v1::TypeCode::STRUCT,
            null_struct_proto.first.code());

  auto const& field0 = p.first.struct_type().fields(0);
  EXPECT_EQ("", field0.name());
  EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, field0.type().code());
  EXPECT_EQ(3.14, p.second.list_value().values(0).number_value());

  auto const& field1 = p.first.struct_type().fields(1);
  EXPECT_EQ("foo", field1.name());
  EXPECT_EQ(google::spanner::v1::TypeCode::INT64, field1.type().code());
  EXPECT_EQ("42", p.second.list_value().values(1).string_value());
}

void SetProtoKind(Value& v, google::protobuf::NullValue x) {
  auto p = internal::ToProto(v);
  p.second.set_null_value(x);
  v = internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, double x) {
  auto p = internal::ToProto(v);
  p.second.set_number_value(x);
  v = internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, char const* x) {
  auto p = internal::ToProto(v);
  p.second.set_string_value(x);
  v = internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, bool x) {
  auto p = internal::ToProto(v);
  p.second.set_bool_value(x);
  v = internal::FromProto(p.first, p.second);
}

void ClearProtoKind(Value& v) {
  auto p = internal::ToProto(v);
  p.second.clear_kind();
  v = internal::FromProto(p.first, p.second);
}

TEST(Value, GetBadBool) {
  Value v(true);
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<bool>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<bool>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<bool>().ok());

  SetProtoKind(v, "hello");
  EXPECT_FALSE(v.get<bool>().ok());
}

TEST(Value, GetBadDouble) {
  Value v(0.0);
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<double>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<double>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<double>().ok());

  SetProtoKind(v, "bad string");
  EXPECT_FALSE(v.get<double>().ok());
}

TEST(Value, GetBadString) {
  Value v("hello");
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<std::string>().ok());
}

TEST(Value, GetBadBytes) {
  Value v(Bytes("hello"));
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<Bytes>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<Bytes>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<Bytes>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<Bytes>().ok());
}

TEST(Value, GetBadNumeric) {
  Value v(MakeNumeric(0).value());
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<std::string>().ok());

  SetProtoKind(v, "");
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, "123blah");
  EXPECT_FALSE(v.get<std::int64_t>().ok());
}

TEST(Value, GetBadInt) {
  Value v(42);
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, "");
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<std::int64_t>().ok());

  SetProtoKind(v, "123blah");
  EXPECT_FALSE(v.get<std::int64_t>().ok());
}

TEST(Value, GetBadTimestamp) {
  Value v(Timestamp{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<Timestamp>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<Timestamp>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<Timestamp>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<Timestamp>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<Timestamp>().ok());
}

TEST(Value, GetBadCommitTimestamp) {
  Value v(CommitTimestamp{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<CommitTimestamp>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<CommitTimestamp>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<CommitTimestamp>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<CommitTimestamp>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<CommitTimestamp>().ok());
}

TEST(Value, GetBadDate) {
  Value v(absl::CivilDay{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<absl::CivilDay>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<absl::CivilDay>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<absl::CivilDay>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<absl::CivilDay>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<absl::CivilDay>().ok());
}

TEST(Value, GetBadOptional) {
  Value v(absl::optional<double>{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<absl::optional<double>>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<absl::optional<double>>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<absl::optional<double>>().ok());
}

TEST(Value, GetBadArray) {
  Value v(std::vector<double>{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<std::vector<double>>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<std::vector<double>>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<std::vector<double>>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<std::vector<double>>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<std::vector<double>>().ok());
}

TEST(Value, GetBadStruct) {
  Value v(std::tuple<bool>{});
  ClearProtoKind(v);
  EXPECT_FALSE(v.get<std::tuple<bool>>().ok());

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_FALSE(v.get<std::tuple<bool>>().ok());

  SetProtoKind(v, true);
  EXPECT_FALSE(v.get<std::tuple<bool>>().ok());

  SetProtoKind(v, 0.0);
  EXPECT_FALSE(v.get<std::tuple<bool>>().ok());

  SetProtoKind(v, "blah");
  EXPECT_FALSE(v.get<std::tuple<bool>>().ok());
}

TEST(Value, CommitTimestamp) {
  auto const v = Value(CommitTimestamp{});
  auto tv = internal::ToProto(v);
  EXPECT_EQ(google::spanner::v1::TypeCode::TIMESTAMP, tv.first.code());

  auto constexpr kText = R"pb(
    string_value: "spanner.commit_timestamp()"
  )pb";
  google::protobuf::Value pv;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &pv));
  EXPECT_THAT(tv.second, IsProtoEqual(pv));

  auto good = v.get<CommitTimestamp>();
  EXPECT_STATUS_OK(good);
  EXPECT_EQ(CommitTimestamp{}, *good);

  auto bad = v.get<Timestamp>();
  EXPECT_FALSE(bad.ok());
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
  auto const alphahex = [](std::ostream& os) -> std::ostream& {
    return os << std::boolalpha << std::hex;
  };

  auto const inf = std::numeric_limits<double>::infinity();
  auto const nan = std::nan("NaN");

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
      {Value(inf), "inf", normal},
      {Value(-inf), "-inf", normal},
      {Value(nan), "nan", normal},
      {Value(""), "", normal},
      {Value("foo"), "foo", normal},
      {Value("NULL"), "NULL", normal},
      {Value(Bytes(std::string("DEADBEEF"))), R"(B"DEADBEEF")", normal},
      {Value(MakeNumeric(1234567890).value()), "1234567890", normal},
      {Value(absl::CivilDay()), "1970-01-01", normal},
      {Value(Timestamp()), "1970-01-01T00:00:00Z", normal},

      // Tests string quoting: No quotes for scalars; quotes within aggregates
      {Value(""), "", normal},
      {Value("foo"), "foo", normal},
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])", normal},
      {Value(std::make_tuple("foo")), R"(("foo"))", normal},
      {Value(std::make_tuple(std::make_pair("foo", "bar"))),
       R"(("foo": "bar"))", normal},
      {Value(std::vector<std::string>{"\"a\"", "\"b\""}),
       R"(["\"a\"", "\"b\""])", normal},
      {Value(std::make_tuple("\"foo\"")), R"(("\"foo\""))", normal},
      {Value(std::make_tuple(std::make_pair("\"foo\"", "\"bar\""))),
       R"(("\"foo\"": "\"bar\""))", normal},

      // Tests null values
      {MakeNullValue<bool>(), "NULL", normal},
      {MakeNullValue<std::int64_t>(), "NULL", normal},
      {MakeNullValue<double>(), "NULL", normal},
      {MakeNullValue<std::string>(), "NULL", normal},
      {MakeNullValue<Bytes>(), "NULL", normal},
      {MakeNullValue<Numeric>(), "NULL", normal},
      {MakeNullValue<absl::CivilDay>(), "NULL", normal},
      {MakeNullValue<Timestamp>(), "NULL", normal},

      // Tests arrays
      {Value(std::vector<bool>{false, true}), "[0, 1]", normal},
      {Value(std::vector<bool>{false, true}), "[false, true]", boolalpha},
      {Value(std::vector<std::int64_t>{10, 11}), "[10, 11]", normal},
      {Value(std::vector<std::int64_t>{10, 11}), "[a, b]", hex},
      {Value(std::vector<double>{1.0, 2.0}), "[1, 2]", normal},
      {Value(std::vector<double>{1.0, 2.0}), "[1.000, 2.000]", float4},
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])", normal},
      {Value(std::vector<Bytes>{2}), R"([B"", B""])", normal},
      {Value(std::vector<Numeric>{2}), "[0, 0]", normal},
      {Value(std::vector<absl::CivilDay>{2}), "[1970-01-01, 1970-01-01]",
       normal},
      {Value(std::vector<Timestamp>{1}), "[1970-01-01T00:00:00Z]", normal},
      {Value(std::vector<absl::optional<double>>{1, {}, 2}), "[1, NULL, 2]",
       normal},

      // Tests null arrays
      {MakeNullValue<std::vector<bool>>(), "NULL", normal},
      {MakeNullValue<std::vector<std::int64_t>>(), "NULL", normal},
      {MakeNullValue<std::vector<double>>(), "NULL", normal},
      {MakeNullValue<std::vector<std::string>>(), "NULL", normal},
      {MakeNullValue<std::vector<Bytes>>(), "NULL", normal},
      {MakeNullValue<std::vector<Numeric>>(), "NULL", normal},
      {MakeNullValue<std::vector<absl::CivilDay>>(), "NULL", normal},
      {MakeNullValue<std::vector<Timestamp>>(), "NULL", normal},

      // Tests structs
      {Value(std::make_tuple(true, 123)), "(1, 123)", normal},
      {Value(std::make_tuple(true, 123)), "(true, 7b)", alphahex},
      {Value(std::make_tuple(std::make_pair("A", true),
                             std::make_pair("B", 123))),
       R"(("A": 1, "B": 123))", normal},
      {Value(std::make_tuple(std::make_pair("A", true),
                             std::make_pair("B", 123))),
       R"(("A": true, "B": 7b))", alphahex},
      {Value(std::make_tuple(
           std::vector<std::int64_t>{10, 11, 12},
           std::make_pair("B", std::vector<std::int64_t>{13, 14, 15}))),
       R"(([10, 11, 12], "B": [13, 14, 15]))", normal},
      {Value(std::make_tuple(
           std::vector<std::int64_t>{10, 11, 12},
           std::make_pair("B", std::vector<std::int64_t>{13, 14, 15}))),
       R"(([a, b, c], "B": [d, e, f]))", hex},
      {Value(std::make_tuple(std::make_tuple(
           std::make_tuple(std::vector<std::int64_t>{10, 11, 12})))),
       "((([10, 11, 12])))", normal},
      {Value(std::make_tuple(std::make_tuple(
           std::make_tuple(std::vector<std::int64_t>{10, 11, 12})))),
       "((([a, b, c])))", hex},

      // Tests struct with null members
      {Value(std::make_tuple(absl::optional<bool>{})), "(NULL)", normal},
      {Value(std::make_tuple(absl::optional<bool>{}, 123)), "(NULL, 123)",
       normal},
      {Value(std::make_tuple(absl::optional<bool>{}, 123)), "(NULL, 7b)", hex},
      {Value(std::make_tuple(absl::optional<bool>{},
                             absl::optional<std::int64_t>{})),
       "(NULL, NULL)", normal},

      // Tests null structs
      {MakeNullValue<std::tuple<bool>>(), "NULL", normal},
      {MakeNullValue<std::tuple<bool, std::int64_t>>(), "NULL", normal},
      {MakeNullValue<std::tuple<bool, std::string>>(), "NULL", normal},
      {MakeNullValue<std::tuple<double, Bytes, Timestamp>>(), "NULL", normal},
      {MakeNullValue<std::tuple<Numeric, absl::CivilDay>>(), "NULL", normal},
      {MakeNullValue<std::tuple<std::vector<bool>>>(), "NULL", normal},
  };

  for (auto const& tc : test_case) {
    std::stringstream ss;
    tc.manip(ss) << tc.value;
    EXPECT_EQ(ss.str(), tc.expected);
  }
}

// Ensures that the following expressions produce the same output.
//
// `os << t`
// `os << Value(t)`
//
template <typename T>
void StreamMatchesValueStream(T t) {
  std::ostringstream ss1;
  ss1 << t;
  std::ostringstream ss2;
  ss2 << Value(std::move(t));
  EXPECT_EQ(ss1.str(), ss2.str());
}

TEST(Value, OutputStreamMatchesT) {
  // bool
  StreamMatchesValueStream(false);
  StreamMatchesValueStream(true);

  // std::int64_t
  StreamMatchesValueStream(-1);
  StreamMatchesValueStream(0);
  StreamMatchesValueStream(1);

  // double
  StreamMatchesValueStream(0.0);
  StreamMatchesValueStream(3.14);
  StreamMatchesValueStream(std::nan("NaN"));
  StreamMatchesValueStream(std::numeric_limits<double>::infinity());
  StreamMatchesValueStream(-std::numeric_limits<double>::infinity());

  // std::string
  StreamMatchesValueStream("");
  StreamMatchesValueStream("foo");
  StreamMatchesValueStream("\"foo\"");

  // Bytes
  StreamMatchesValueStream(Bytes());
  StreamMatchesValueStream(Bytes("foo"));

  // Numeric
  StreamMatchesValueStream(MakeNumeric("999").value());
  StreamMatchesValueStream(MakeNumeric(3.14159).value());
  StreamMatchesValueStream(MakeNumeric(42).value());

  // Date
  StreamMatchesValueStream(absl::CivilDay(1, 1, 1));
  StreamMatchesValueStream(absl::CivilDay());
  StreamMatchesValueStream(absl::CivilDay(9999, 12, 31));

  // Timestamp
  StreamMatchesValueStream(Timestamp());
  StreamMatchesValueStream(MakeTimestamp(MakeTimePoint(1, 1)).value());

  // std::vector<T>
  // Not included, because a raw vector cannot be streamed.

  // std::tuple<...>
  // Not included, because a raw tuple cannot be streamed.
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
