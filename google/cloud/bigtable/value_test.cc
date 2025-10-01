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
#include "google/cloud/bigtable/internal/tuple_utils.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/cord.h"
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
using ::google::cloud::testing_util::IsOkAndHolds;
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

  for (auto x : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
    SCOPED_TRACE("Testing: double " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<double>(5, x));
    std::vector<absl::optional<double>> v(5, x);
    v.resize(10);
    TestBasicSemantics(v);
  }

  for (auto x : {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F}) {
    SCOPED_TRACE("Testing: float " + std::to_string(x));
    TestBasicSemantics(x);
    TestBasicSemantics(std::vector<float>(5, x));
    std::vector<absl::optional<float>> v(5, x);
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

  for (auto ts : TestTimes()) {
    SCOPED_TRACE("Testing: google::cloud::bigtable::Timestamp " +
                 bigtable_internal::TimestampToRFC3339(ts));
    std::cout << "Testing: google::cloud::bigtable::Timestamp "
              << bigtable_internal::TimestampToRFC3339(ts) << std::endl;
    TestBasicSemantics(ts);
    std::vector<Timestamp> v(5, ts);
    TestBasicSemantics(v);
    std::vector<absl::optional<Timestamp>> ov(5, ts);
    ov.resize(10);
    TestBasicSemantics(ov);
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
}

TEST(Value, ArrayValueBasedEquality) {
  std::vector<Value> test_cases = {
      Value(std::vector<double>{1.2, 3.4}),
      Value(std::make_tuple(1.2, 3.4)),

      // empty containers
      Value(std::vector<double>()),
      Value(std::make_tuple()),
  };

  for (size_t i = 0; i < test_cases.size(); i++) {
    auto const& tc1 = test_cases[i];
    for (size_t j = 0; j < test_cases.size(); j++) {
      auto const& tc2 = test_cases[j];
      // Compares tc1 to tc2, which ensures that different "kinds" of
      // value are never equal.
      if (i == j) {
        EXPECT_EQ(tc1, tc2);
        continue;
      }
      EXPECT_NE(tc1, tc2);
    }
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

// The next tests (RvalueGet*) assume std::string is the underlying type of
// protobuf accessors for string values. In situations where the underlying type
// is absl::Cord, the assumptions are no longer valid and checking the moved
// from state of the std::string is even less of a good idea than normal.
template <typename T,
          typename U = decltype(std::declval<google::bigtable::v2::Value>()
                                    .string_value()),
          typename std::enable_if<
              std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                           std::string>::value>::type* = nullptr>
StatusOr<T> MovedFromString(Value const& v) {
  return v.get<T>();
}

template <typename T,
          typename U = decltype(std::declval<google::bigtable::v2::Value>()
                                    .string_value()),
          typename std::enable_if<
              std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                           absl::Cord>::value>::type* = nullptr,
          typename std::enable_if_t<
              !std::is_same<T, std::vector<std::string>>::value, int> = 0>
StatusOr<T> MovedFromString(Value const&) {
  return T{""};
}

template <typename T,
          typename U = decltype(std::declval<google::bigtable::v2::Value>()
                                    .string_value()),
          typename std::enable_if<
              std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                           absl::Cord>::value>::type* = nullptr,
          typename std::enable_if_t<
              std::is_same<T, std::vector<std::string>>::value, int> = 0>
StatusOr<T> MovedFromString(Value const& v) {
  auto v2 = v.get<T>();
  if (!v2.ok()) {
    return v2.status();
  }
  return T{v2->size(), std::string{""}};
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
  s = MovedFromString<Type>(v);
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
  s = MovedFromString<Type>(v);
  ASSERT_STATUS_OK(s);
  EXPECT_EQ("", **s);
}

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// bigtable::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetVectorString) {
  using Type = std::vector<std::string>;
  Type const data(128, std::string(128, 'x'));
  Value v(data);

  auto s = v.get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = MovedFromString<Type>(v);
  ASSERT_STATUS_OK(s);
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
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  s = std::move(v).get<Type>();
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(data, *s);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  s = MovedFromString<Type>(v);
  ASSERT_STATUS_OK(s);
  EXPECT_EQ(Type({"name", ""}, ""), *s);
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

  std::vector<char const*> vec = {"foo", "bar"};
  Value v_vec(vec);
  EXPECT_STATUS_OK(v_vec.get<std::vector<std::string>>());
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

TEST(Value, BigtableArray) {
  using ArrayInt64 = std::vector<std::int64_t>;
  using ArrayDouble = std::vector<double>;
  using ArrayFloat = std::vector<float>;

  ArrayInt64 const empty = {};
  Value const ve(empty);
  EXPECT_EQ(ve, ve);
  ASSERT_STATUS_OK(ve.get<ArrayInt64>());
  EXPECT_THAT(ve.get<ArrayDouble>(), Not(IsOk()));
  EXPECT_EQ(empty, *ve.get<ArrayInt64>());

  ArrayInt64 const ai = {1, 2, 3};
  Value const vi(ai);
  EXPECT_EQ(vi, vi);
  ASSERT_STATUS_OK(vi.get<ArrayInt64>());
  EXPECT_THAT(vi.get<ArrayDouble>(), Not(IsOk()));
  EXPECT_EQ(ai, *vi.get<ArrayInt64>());

  ArrayDouble const ad = {1.0, 2.0, 3.0};
  Value const vd(ad);
  EXPECT_EQ(vd, vd);
  EXPECT_NE(vi, vd);
  EXPECT_THAT(vd.get<ArrayInt64>(), Not(IsOk()));
  ASSERT_STATUS_OK(vd.get<ArrayDouble>());
  EXPECT_EQ(ad, *vd.get<ArrayDouble>());

  ArrayFloat const af = {1.0, 2.0, 3.0};
  Value const vf(af);
  EXPECT_EQ(vf, vf);
  EXPECT_NE(vi, vf);
  EXPECT_THAT(vf.get<ArrayInt64>(), Not(IsOk()));
  ASSERT_STATUS_OK(vf.get<ArrayFloat>());
  EXPECT_EQ(af, *vf.get<ArrayFloat>());

  Value const null_vi = MakeNullValue<ArrayInt64>();
  EXPECT_EQ(null_vi, null_vi);
  EXPECT_NE(null_vi, vi);
  EXPECT_NE(null_vi, vd);
  EXPECT_THAT(null_vi.get<ArrayInt64>(), Not(IsOk()));
  EXPECT_THAT(null_vi.get<ArrayDouble>(), Not(IsOk()));

  Value const null_vd = MakeNullValue<ArrayDouble>();
  EXPECT_EQ(null_vd, null_vd);
  EXPECT_NE(null_vd, null_vi);
  EXPECT_NE(null_vd, vd);
  EXPECT_NE(null_vd, vi);
  EXPECT_THAT(null_vd.get<ArrayDouble>(), Not(IsOk()));
  EXPECT_THAT(null_vd.get<ArrayInt64>(), Not(IsOk()));

  Value const null_vf = MakeNullValue<ArrayFloat>();
  EXPECT_EQ(null_vf, null_vf);
  EXPECT_NE(null_vf, null_vi);
  EXPECT_NE(null_vf, vf);
  EXPECT_NE(null_vf, vi);
  EXPECT_THAT(null_vf.get<ArrayFloat>(), Not(IsOk()));
  EXPECT_THAT(null_vf.get<ArrayInt64>(), Not(IsOk()));
}

TEST(Value, BigtableStruct) {
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
  ASSERT_STATUS_OK(v1.get<T1>());
  EXPECT_EQ(tup1, *v1.get<T1>());
  EXPECT_EQ(v1, v1);

  // Verify we can extract tuple elements even if they're wrapped in a pair.
  auto const pair0 = v1.get<tuple<pair<string, bool>, int64_t>>();
  ASSERT_STATUS_OK(pair0);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair0).second);
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair0));
  auto const pair1 = v1.get<tuple<bool, pair<string, int64_t>>>();
  ASSERT_STATUS_OK(pair1);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair1));
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair1).second);
  auto const pair01 =
      v1.get<tuple<pair<string, bool>, pair<string, int64_t>>>();
  ASSERT_STATUS_OK(pair01);
  EXPECT_EQ(std::get<0>(tup1), std::get<0>(*pair01).second);
  EXPECT_EQ(std::get<1>(tup1), std::get<1>(*pair01).second);

  auto tup2 = make_tuple(false, make_pair(string("f2"), int64_t{123}));
  using T2 = decltype(tup2);
  Value v2(tup2);
  EXPECT_THAT(v2.get<T2>(), IsOkAndHolds(tup2));
  EXPECT_EQ(v2, v2);
  EXPECT_NE(v2, v1);

  // T1 is lacking field names, but otherwise the same as T2.
  EXPECT_EQ(tup1, *v2.get<T1>());
  EXPECT_NE(tup2, *v1.get<T2>());

  auto tup3 = make_tuple(false, make_pair(string("Other"), int64_t{123}));
  using T3 = decltype(tup3);
  Value v3(tup3);
  EXPECT_THAT(v3.get<T3>(), IsOkAndHolds(tup3));
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
  EXPECT_THAT(v4.get<T3>(), Not(IsOk()));
  EXPECT_THAT(v4.get<T2>(), Not(IsOk()));
  EXPECT_THAT(v4.get<T1>(), Not(IsOk()));

  EXPECT_THAT(v4.get<T4>(), IsOkAndHolds(array_struct));

  auto empty = tuple<>{};
  using T5 = decltype(empty);
  Value v5(empty);
  EXPECT_STATUS_OK(v5.get<T5>());
  EXPECT_THAT(v5.get<T4>(), Not(IsOk()));
  EXPECT_EQ(v5, v5);
  EXPECT_NE(v5, v4);

  EXPECT_THAT(v5.get<T5>(), IsOkAndHolds(empty));

  auto deeply_nested = tuple<tuple<std::vector<absl::optional<bool>>>>{};
  using T6 = decltype(deeply_nested);
  Value v6(deeply_nested);
  EXPECT_STATUS_OK(v6.get<T6>());
  EXPECT_THAT(v6.get<T5>(), Not(IsOk()));
  EXPECT_EQ(v6, v6);
  EXPECT_NE(v6, v5);

  EXPECT_THAT(v6.get<T6>(), IsOkAndHolds(deeply_nested));
}

TEST(Value, BigtableStructWithNull) {
  auto v1 = Value(std::make_tuple(123, true));
  auto v2 = Value(std::make_tuple(123, absl::optional<bool>{}));

  auto protos1 = bigtable_internal::ToProto(v1);
  auto protos2 = bigtable_internal::ToProto(v2);

  // The type protos match for both values, but the value protos DO NOT match.
  EXPECT_THAT(protos1.first, IsProtoEqual(protos2.first));
  EXPECT_THAT(protos1.second, Not(IsProtoEqual(protos2.second)));

  // Now verify that the second value has two fields and the second field
  // contains a NULL value.
  ASSERT_EQ(protos2.second.array_value().values_size(), 2);
  ASSERT_EQ(protos2.second.array_value().values(1).kind_case(),
            google::bigtable::v2::Value::KIND_NOT_SET);
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

TEST(Value, ProtoConversionArray) {
  std::vector<std::int64_t> data{1, 2, 3};
  Value const v(data);
  auto const p = bigtable_internal::ToProto(v);
  EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
  EXPECT_TRUE(p.first.has_array_type());
  EXPECT_TRUE(p.first.array_type().element_type().has_int64_type());
  EXPECT_EQ(1, p.second.array_value().values(0).int_value());
  EXPECT_EQ(2, p.second.array_value().values(1).int_value());
  EXPECT_EQ(3, p.second.array_value().values(2).int_value());
}

TEST(Value, ProtoConversionStruct) {
  auto data = std::make_tuple(3.14, std::make_pair("foo", 42));
  Value const v(data);
  auto const p = bigtable_internal::ToProto(v);
  EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
  EXPECT_TRUE(p.first.has_struct_type());

  Value const null_struct_value(
      MakeNullValue<std::tuple<bool, std::int64_t>>());
  auto const null_struct_proto = bigtable_internal::ToProto(null_struct_value);
  EXPECT_TRUE(p.first.has_struct_type());

  auto const& field0 = p.first.struct_type().fields(0);
  EXPECT_EQ("", field0.field_name());
  EXPECT_TRUE(field0.type().has_float64_type());
  EXPECT_EQ(3.14, p.second.array_value().values(0).float_value());

  auto const& field1 = p.first.struct_type().fields(1);
  EXPECT_EQ("foo", field1.field_name());
  EXPECT_TRUE(field1.type().has_int64_type());
  EXPECT_EQ(42, p.second.array_value().values(1).int_value());
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

void SetProtoKind(Value& v, std::vector<std::int64_t> const& x) {
  auto p = bigtable_internal::ToProto(v);
  auto list = *p.second.mutable_array_value();
  for (auto&& e : x) {
    google::bigtable::v2::Value el;
    el.set_int_value(e);
    *list.add_values() = el;
  }
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, std::tuple<std::int64_t, std::int64_t> const& x) {
  auto p = bigtable_internal::ToProto(v);
  auto list = *p.second.mutable_array_value();
  auto e = std::get<0>(x);
  google::bigtable::v2::Value el;
  el.set_int_value(e);
  *list.add_values() = el;
  el = google::bigtable::v2::Value();
  e = std::get<1>(x);
  el.set_int_value(e);
  *list.add_values() = el;
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<double>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<float>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<std::string>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<Bytes>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<Timestamp>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<absl::CivilDay>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
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

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<absl::optional<double>>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
  EXPECT_THAT(v.get<absl::optional<double>>(), Not(IsOk()));
}

TEST(Value, GetBadArray) {
  Value v(std::vector<double>{});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
  EXPECT_THAT(v.get<std::vector<double>>(), Not(IsOk()));
}

TEST(Value, GetBadStruct) {
  Value v(std::tuple<bool>{});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<std::tuple<bool>>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<std::tuple<bool>>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<std::tuple<bool>>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<std::tuple<bool>>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<std::tuple<bool>>(), Not(IsOk()));

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<std::tuple<double>>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
  EXPECT_THAT(v.get<std::tuple<double>>(), Not(IsOk()));
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
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])", normal},
      {Value(std::vector<std::string>{"\"a\"", "\"b\""}),
       R"(["\"a\"", "\"b\""])", normal},

      // Tests null values
      {MakeNullValue<bool>(), "NULL", normal},
      {MakeNullValue<std::int64_t>(), "NULL", normal},
      {MakeNullValue<double>(), "NULL", normal},
      {MakeNullValue<float>(), "NULL", normal},
      {MakeNullValue<std::string>(), "NULL", normal},
      {MakeNullValue<Bytes>(), "NULL", normal},
      {MakeNullValue<Timestamp>(), "NULL", normal},
      {MakeNullValue<absl::CivilDay>(), "NULL", normal},

      // Tests arrays
      {Value(std::vector<bool>{false, true}), "[0, 1]", normal},
      {Value(std::vector<bool>{false, true}), "[false, true]", boolalpha},
      {Value(std::vector<std::int64_t>{10, 11}), "[10, 11]", normal},
      {Value(std::vector<std::int64_t>{10, 11}), "[a, b]", hex},
      {Value(std::vector<double>{1.0, 2.0}), "[1, 2]", normal},
      {Value(std::vector<double>{1.0, 2.0}), "[1.000, 2.000]", float4},
      {Value(std::vector<float>{1.0F, 2.0F}), "[1, 2]", normal},
      {Value(std::vector<float>{1.0F, 2.0F}), "[1.000, 2.000]", float4},
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])", normal},
      {Value(std::vector<Bytes>{2}), R"([B"", B""])", normal},
      {Value(std::vector<absl::CivilDay>{2}), "[1970-01-01, 1970-01-01]",
       normal},
      {Value(std::vector<Timestamp>{1}), "[1970-01-01T00:00:00Z]", normal},

      // Tests arrays with null elements
      {Value(std::vector<absl::optional<double>>{1, {}, 2}), "[1, NULL, 2]",
       normal},

      // Tests null arrays
      {MakeNullValue<std::vector<bool>>(), "NULL", normal},
      {MakeNullValue<std::vector<std::int64_t>>(), "NULL", normal},
      {MakeNullValue<std::vector<double>>(), "NULL", normal},
      {MakeNullValue<std::vector<float>>(), "NULL", normal},
      {MakeNullValue<std::vector<std::string>>(), "NULL", normal},
      {MakeNullValue<std::vector<Bytes>>(), "NULL", normal},
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
      {MakeNullValue<std::tuple<float, std::string>>(), "NULL", normal},
      {MakeNullValue<std::tuple<double, Bytes, Timestamp>>(), "NULL", normal},
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

  // float
  StreamMatchesValueStream(0.0F);
  StreamMatchesValueStream(3.14F);

  // std::string
  StreamMatchesValueStream("");
  StreamMatchesValueStream("foo");
  StreamMatchesValueStream("\"foo\"");

  // Bytes
  StreamMatchesValueStream(Bytes());
  StreamMatchesValueStream(Bytes("foo"));

  // Date
  StreamMatchesValueStream(absl::CivilDay(1, 1, 1));
  StreamMatchesValueStream(absl::CivilDay());
  StreamMatchesValueStream(absl::CivilDay(9999, 12, 31));

  // Timestamp
  StreamMatchesValueStream(Timestamp());
  StreamMatchesValueStream(MakeTimestamp(MakeTime(1, 1)).value());

  // std::vector<T>
  // Not included, because a raw vector cannot be streamed.

  // std::tuple<...>
  // Not included, because a raw tuple cannot be streamed.
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
