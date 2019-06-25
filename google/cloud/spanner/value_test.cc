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
#include "google/cloud/optional.h"
#include <gmock/gmock.h>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

template <typename T>
void TestBasicSemantics(T init) {
  Value const v{init};

  EXPECT_TRUE(v.is<T>());
  EXPECT_FALSE(v.is_null<T>());
  EXPECT_TRUE(v.get<T>().ok());
  EXPECT_EQ(init, *v.get<T>());
  EXPECT_EQ(init, static_cast<T>(v));

  Value copy = v;
  EXPECT_EQ(copy, v);
  Value const moved = std::move(copy);
  EXPECT_EQ(moved, v);

  // Tests a null Value of type `T`.
  Value const null = MakeNullValue<T>();

  EXPECT_TRUE(null.is<T>());
  EXPECT_TRUE(null.is_null<T>());
  EXPECT_FALSE(null.get<T>().ok());
  EXPECT_TRUE(null.is<optional<T>>());
  EXPECT_TRUE(null.is_null<optional<T>>());
  EXPECT_TRUE(null.get<optional<T>>().ok());
  EXPECT_EQ(optional<T>{}, *null.get<optional<T>>());

  Value copy_null = null;
  EXPECT_EQ(copy_null, null);
  Value const moved_null = std::move(copy_null);
  EXPECT_EQ(moved_null, null);

  // Round-trip from Value -> Proto(s) -> Value
  auto const protos = internal::ToProto(v);
  EXPECT_EQ(v, internal::FromProto(protos.first, protos.second));
}

TEST(Value, BasicSemantics) {
  for (auto x : {false, true}) {
    SCOPED_TRACE("Testing: bool " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<bool>(5, x));
    std::vector<optional<bool>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, max64}) {
    SCOPED_TRACE("Testing: std::int64_t " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<std::int64_t>(5, x));
    std::vector<optional<std::int64_t>> v(5, x);
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
    std::vector<optional<double>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (auto const& x :
       std::vector<std::string>{"", "f", "foo", "12345678901234567"}) {
    SCOPED_TRACE("Testing: std::string " + std::string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<std::string>(5, x));
    std::vector<optional<std::string>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }
}

TEST(Value, DoubleNaN) {
  double const nan = std::nan("NaN");
  Value v{nan};
  EXPECT_TRUE(v.is<double>());
  EXPECT_FALSE(v.is_null<double>());
  EXPECT_TRUE(std::isnan(*v.get<double>()));

  // Since IEEE 754 defines that nan is not equal to itself, then a Value with
  // NaN should not be equal to itself.
  EXPECT_NE(nan, nan);
  EXPECT_NE(v, v);
}

TEST(Value, ConstructionFromLiterals) {
  Value v_int64(42);
  EXPECT_TRUE(v_int64.is<std::int64_t>());
  EXPECT_EQ(42, *v_int64.get<std::int64_t>());

  Value v_string("hello");
  EXPECT_TRUE(v_string.is<std::string>());
  EXPECT_EQ("hello", *v_string.get<std::string>());

  std::vector<char const*> vec = {"foo", "bar"};
  Value v_vec(vec);
  EXPECT_TRUE(v_vec.is<std::vector<std::string>>());

  std::tuple<char const*, char const*> tup = std::make_tuple("foo", "bar");
  Value v_tup(tup);
  EXPECT_TRUE((v_tup.is<std::tuple<std::string, std::string>>()));

  auto named_field = std::make_tuple(false, std::make_pair("f1", 42));
  Value v_named_field(named_field);
  EXPECT_TRUE(
      (v_named_field
           .is<std::tuple<bool, std::pair<std::string, std::int64_t>>>()));
}

TEST(Value, MixingTypes) {
  using A = bool;
  using B = std::int64_t;

  Value a(A{});
  EXPECT_TRUE(a.is<A>());
  EXPECT_FALSE(a.is_null<A>());
  EXPECT_TRUE(a.get<A>().ok());
  EXPECT_FALSE(a.is<B>());
  EXPECT_FALSE(a.is_null<B>());
  EXPECT_FALSE(a.get<B>().ok());

  Value null_a = MakeNullValue<A>();
  EXPECT_TRUE(null_a.is<A>());
  EXPECT_TRUE(null_a.is_null<A>());
  EXPECT_FALSE(null_a.get<A>().ok());
  EXPECT_FALSE(null_a.is<B>());
  EXPECT_FALSE(null_a.is_null<B>());
  EXPECT_FALSE(null_a.get<B>().ok());

  EXPECT_NE(null_a, a);

  Value b(B{});
  EXPECT_TRUE(b.is<B>());
  EXPECT_FALSE(b.is_null<B>());
  EXPECT_TRUE(b.get<B>().ok());
  EXPECT_FALSE(b.is<A>());
  EXPECT_FALSE(b.is_null<A>());
  EXPECT_FALSE(b.get<A>().ok());

  EXPECT_NE(b, a);
  EXPECT_NE(b, null_a);

  Value null_b = MakeNullValue<B>();
  EXPECT_TRUE(null_b.is<B>());
  EXPECT_TRUE(null_b.is_null<B>());
  EXPECT_FALSE(null_b.get<B>().ok());
  EXPECT_FALSE(null_b.is<A>());
  EXPECT_FALSE(null_b.is_null<A>());
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
  EXPECT_TRUE(ve.is<ArrayInt64>());
  EXPECT_FALSE(ve.is_null<ArrayInt64>());
  EXPECT_FALSE(ve.is<ArrayDouble>());
  EXPECT_TRUE(ve.get<ArrayInt64>().ok());
  EXPECT_FALSE(ve.get<ArrayDouble>().ok());
  EXPECT_EQ(empty, *ve.get<ArrayInt64>());
  EXPECT_EQ(empty, static_cast<ArrayInt64>(ve));

  ArrayInt64 const ai = {1, 2, 3};
  Value const vi(ai);
  EXPECT_EQ(vi, vi);
  EXPECT_TRUE(vi.is<ArrayInt64>());
  EXPECT_FALSE(vi.is_null<ArrayInt64>());
  EXPECT_FALSE(vi.is<ArrayDouble>());
  EXPECT_TRUE(vi.get<ArrayInt64>().ok());
  EXPECT_FALSE(vi.get<ArrayDouble>().ok());
  EXPECT_EQ(ai, *vi.get<ArrayInt64>());
  EXPECT_EQ(ai, static_cast<ArrayInt64>(vi));

  ArrayDouble const ad = {1.0, 2.0, 3.0};
  Value const vd(ad);
  EXPECT_EQ(vd, vd);
  EXPECT_NE(vi, vd);
  EXPECT_TRUE(vd.is<ArrayDouble>());
  EXPECT_FALSE(vd.is_null<ArrayDouble>());
  EXPECT_FALSE(vd.is<ArrayInt64>());
  EXPECT_TRUE(vd.get<ArrayDouble>().ok());
  EXPECT_EQ(ad, *vd.get<ArrayDouble>());
  EXPECT_EQ(ad, static_cast<ArrayDouble>(vd));

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
  EXPECT_TRUE(v1.is<T1>());
  EXPECT_FALSE(v1.is_null<T1>());
  EXPECT_TRUE(v1.get<T1>().ok());
  EXPECT_EQ(tup1, *v1.get<T1>());
  EXPECT_EQ(v1, v1);

  // Verify that wrapping an element in a pair doesn't affect its C++ type.
  EXPECT_TRUE((v1.is<tuple<bool, int64_t>>()));
  EXPECT_TRUE((v1.is<tuple<pair<string, bool>, int64_t>>()));
  EXPECT_TRUE((v1.is<tuple<bool, pair<string, int64_t>>>()));
  EXPECT_TRUE((v1.is<tuple<pair<string, bool>, pair<string, int64_t>>>()));

  auto tup2 = make_tuple(false, make_pair(string("f2"), int64_t{123}));
  using T2 = decltype(tup2);
  Value v2(tup2);
  EXPECT_TRUE(v2.is<T2>());
  EXPECT_FALSE(v2.is_null<T2>());
  EXPECT_TRUE(v2.get<T2>().ok());
  EXPECT_EQ(tup2, *v2.get<T2>());
  EXPECT_EQ(v2, v2);
  EXPECT_NE(v2, v1);

  // T1 is lacking field names, but otherwise the same as T2.
  EXPECT_EQ(tup1, *v2.get<T1>());
  EXPECT_NE(tup2, *v1.get<T2>());

  auto tup3 = make_tuple(false, make_pair(string("Other"), int64_t{123}));
  using T3 = decltype(tup3);
  Value v3(tup3);
  EXPECT_TRUE(v3.is<T3>());
  EXPECT_FALSE(v3.is_null<T3>());
  EXPECT_TRUE(v3.get<T3>().ok());
  EXPECT_EQ(tup3, *v3.get<T3>());
  EXPECT_EQ(v3, v3);
  EXPECT_NE(v3, v2);
  EXPECT_NE(v3, v1);

  static_assert(std::is_same<T2, T3>::value, "Only diff is field name");

  // v1 != v2, yet T2 works with v1 and vice versa
  EXPECT_NE(v1, v2);
  EXPECT_TRUE(v1.is<T2>());
  EXPECT_FALSE(v1.is_null<T2>());
  EXPECT_TRUE(v2.is<T1>());
  EXPECT_FALSE(v2.is_null<T1>());

  Value v_null(optional<T1>{});
  EXPECT_TRUE(v_null.is<T1>());
  EXPECT_TRUE(v_null.is<T2>());
  EXPECT_TRUE(v_null.is_null<T1>());
  EXPECT_TRUE(v_null.is_null<T2>());

  EXPECT_NE(v1, v_null);
  EXPECT_NE(v2, v_null);

  auto array_struct = std::vector<T3>{
      T3{false, {"age", 1}},
      T3{true, {"age", 2}},
      T3{false, {"age", 3}},
  };
  using T4 = decltype(array_struct);
  Value v4(array_struct);
  EXPECT_TRUE(v4.is<T4>());
  EXPECT_FALSE(v4.is<T3>());
  EXPECT_FALSE(v4.is<T2>());
  EXPECT_FALSE(v4.is<T1>());

  EXPECT_FALSE(v4.is_null<T4>());
  EXPECT_TRUE(v4.get<T4>().ok());
  EXPECT_EQ(array_struct, *v4.get<T4>());

  auto empty = tuple<>{};
  using T5 = decltype(empty);
  Value v5(empty);
  EXPECT_TRUE(v5.is<T5>());
  EXPECT_FALSE(v5.is<T4>());
  EXPECT_EQ(v5, v5);
  EXPECT_NE(v5, v4);

  EXPECT_FALSE(v5.is_null<T5>());
  EXPECT_TRUE(v5.get<T5>().ok());
  EXPECT_EQ(empty, *v5.get<T5>());

  auto crazy = tuple<tuple<std::vector<optional<bool>>>>{};
  using T6 = decltype(crazy);
  Value v6(crazy);
  EXPECT_TRUE(v6.is<T6>());
  EXPECT_FALSE(v6.is<T5>());
  EXPECT_EQ(v6, v6);
  EXPECT_NE(v6, v5);

  EXPECT_FALSE(v6.is_null<T6>());
  EXPECT_TRUE(v6.get<T6>().ok());
  EXPECT_EQ(crazy, *v6.get<T6>());
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

  auto const& field0 = p.first.struct_type().fields(0);
  EXPECT_EQ("", field0.name());
  EXPECT_EQ(google::spanner::v1::TypeCode::FLOAT64, field0.type().code());
  EXPECT_EQ(3.14, p.second.list_value().values(0).number_value());

  auto const& field1 = p.first.struct_type().fields(1);
  EXPECT_EQ("foo", field1.name());
  EXPECT_EQ(google::spanner::v1::TypeCode::INT64, field1.type().code());
  EXPECT_EQ("42", p.second.list_value().values(1).string_value());
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
