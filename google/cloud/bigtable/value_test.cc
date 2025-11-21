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
#include <google/protobuf/text_format.h>
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
using ::google::protobuf::TextFormat;
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
      Value(std::unordered_map<std::int64_t, std::string>{{12, "foo"},
                                                          {34, "bar"}}),

      // empty containers
      Value(std::vector<double>()),
      Value(std::make_tuple()),
      Value(std::unordered_map<std::int64_t, std::string>()),
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

TEST(Value, UnsortedKeysMapEquality) {
  std::vector<std::pair<Value, Value>> const test_cases = {
      {
          Value(std::unordered_map<std::int64_t, std::string>{{12, "foo"},
                                                              {34, "bar"}}),
          Value(std::unordered_map<std::int64_t, std::string>{{34, "bar"},
                                                              {12, "foo"}}),
      },
      {
          Value(std::unordered_map<std::string, std::string>{{"12", "foo"},
                                                             {"34", "bar"}}),
          Value(std::unordered_map<std::string, std::string>{{"34", "bar"},
                                                             {"12", "foo"}}),
      },
      {
          Value(std::unordered_map<Bytes, std::string>{{Bytes("12"), "foo"},
                                                       {Bytes("34"), "bar"}}),
          Value(std::unordered_map<Bytes, std::string>{{Bytes("34"), "bar"},
                                                       {Bytes("12"), "foo"}}),
      }

  };
  for (auto const& tc : test_cases) {
    EXPECT_EQ(tc.first, tc.second);
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
      {Value(std::unordered_map<std::int64_t, std::string>{{123, "foo"}}),
       Value(std::unordered_map<std::int64_t, std::string>{{456, "bar"}})}};

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

template <
    typename T,
    typename U =
        decltype(std::declval<google::bigtable::v2::Value>().string_value()),
    typename std::enable_if<
        std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                     absl::Cord>::value>::type* = nullptr,
    typename std::enable_if_t<
        absl::disjunction<std::is_same<T, std::string>,
                          std::is_same<T, absl::optional<std::string>>>::value,
        int> = 0>

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

template <typename T,
          typename U = decltype(std::declval<google::bigtable::v2::Value>()
                                    .string_value()),
          typename std::enable_if<
              std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                           absl::Cord>::value>::type* = nullptr,
          typename std::enable_if_t<
              std::is_same<T, std::tuple<std::pair<std::string, std::string>,
                                         std::string>>::value,
              int> = 0>
StatusOr<T> MovedFromString(Value const&) {
  return std::tuple<std::pair<std::string, std::string>, std::string>{
      std::pair<std::string, std::string>("name", ""), ""};
}
template <
    typename T,
    typename U =
        decltype(std::declval<google::bigtable::v2::Value>().string_value()),
    typename std::enable_if<
        std::is_same<std::remove_cv_t<std::remove_reference_t<U>>,
                     absl::Cord>::value>::type* = nullptr,
    typename std::enable_if_t<
        std::is_same<T, std::unordered_map<std::string, std::string>>::value,
        int> = 0>
StatusOr<T> MovedFromString(Value const&) {
  return std::unordered_map<std::string, std::string>{{{"", ""}, {"", ""}}};
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
// bigtable::Value::get<T>() correctly handles moves. If this test ever breaks
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

// NOTE: This test relies on unspecified behavior about the moved-from state
// of std::string. Specifically, this test relies on the fact that "large"
// strings, when moved-from, end up empty. And we use this fact to verify that
// bigtable::Value::get<T>() correctly handles moves. If this test ever breaks
// on some platform, we could probably delete this, unless we can think of a
// better way to test move semantics.
TEST(Value, RvalueGetMapString) {
  using Type = std::unordered_map<std::string, std::string>;
  Type data{{"foo", std::string(128, 'x')}, {"bar", std::string(128, 'y')}};
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
  EXPECT_EQ(Type({{"", ""}, {"", ""}}), *s);
}

TEST(Value, BytesRelationalOperators) {
  Bytes const b1(std::string(1, '\x00'));
  Bytes const b2(std::string(1, '\xff'));

  EXPECT_EQ(b1, b1);
  EXPECT_NE(b1, b2);

  // tests comparison operators
  Bytes const b3(std::string("a"));
  Bytes const b4(std::string("b"));
  EXPECT_LT(b3, b4);
  EXPECT_LE(b3, b4);
  EXPECT_LE(b3, b3);
  EXPECT_GT(b4, b3);
  EXPECT_GE(b4, b3);
  EXPECT_GE(b4, b4);
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

  std::tuple<std::string, std::int64_t> tup = std::make_tuple("foo", 123);
  Value v_tup(tup);
  auto v_tup_res = v_tup.get<std::tuple<std::string, std::int64_t>>();
  EXPECT_STATUS_OK(v_tup_res);

  std::unordered_map<std::int64_t, std::string> m = {{12, "foo"}, {34, "bar"}};
  Value v_map(m);
  auto v_map_res = v_map.get<std::unordered_map<std::int64_t, std::string>>();
  EXPECT_STATUS_OK(v_map_res);
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

TEST(Value, BigtableMap) {
  // Using declarations to shorten the tests, making them more readable.
  using std::int64_t;
  using std::string;
  using std::unordered_map;

  auto map1 = unordered_map<string, int64_t>{{"foo", 1}, {"bar", 2}};
  using T1 = decltype(map1);
  Value v1(map1);
  ASSERT_STATUS_OK(v1.get<T1>());
  EXPECT_EQ(map1, *v1.get<T1>());
  EXPECT_EQ(v1, v1);

  auto map2 = unordered_map<string, int64_t>{{"baz", 3}, {"qux", 4}};
  using T2 = decltype(map2);
  Value v2(map2);
  EXPECT_THAT(v2.get<T2>(), IsOkAndHolds(map2));
  EXPECT_EQ(v2, v2);
  EXPECT_NE(v2, v1);

  EXPECT_EQ(map2, *v2.get<T1>());
  EXPECT_NE(map2, *v1.get<T2>());

  static_assert(std::is_same<T1, T2>::value,
                "Two maps with same type and different values");

  // v1 != v2, yet T2 works with v1 and vice versa
  EXPECT_NE(v1, v2);
  EXPECT_STATUS_OK(v1.get<T2>());
  EXPECT_STATUS_OK(v2.get<T1>());

  Value v_null(absl::optional<T1>{});
  EXPECT_FALSE(v_null.get<absl::optional<T1>>()->has_value());
  EXPECT_FALSE(v_null.get<absl::optional<T2>>()->has_value());

  EXPECT_NE(v1, v_null);
  EXPECT_NE(v2, v_null);

  auto array_map = std::vector<T2>{
      T2{{"foo2", 1}},
      T2{{"bar2", 2}},
      T2{{"baz2", 3}},
  };
  using T3 = decltype(array_map);
  Value v3(array_map);
  EXPECT_STATUS_OK(v3.get<T3>());
  EXPECT_THAT(v3.get<T2>(), Not(IsOk()));
  EXPECT_THAT(v3.get<T1>(), Not(IsOk()));

  EXPECT_THAT(v3.get<T3>(), IsOkAndHolds(array_map));

  auto empty = unordered_map<Bytes, std::string>{};
  using T4 = decltype(empty);
  Value v4(empty);
  EXPECT_STATUS_OK(v4.get<T4>());
  EXPECT_THAT(v4.get<T3>(), Not(IsOk()));
  EXPECT_EQ(v4, v4);
  EXPECT_NE(v4, v3);

  EXPECT_THAT(v4.get<T4>(), IsOkAndHolds(empty));

  auto deeply_nested = unordered_map<
      std::int64_t,
      std::unordered_map<std::string, std::vector<std::string>>>{};
  using T5 = decltype(deeply_nested);
  Value v5(deeply_nested);
  EXPECT_STATUS_OK(v5.get<T5>());
  EXPECT_THAT(v5.get<T4>(), Not(IsOk()));
  EXPECT_EQ(v5, v5);
  EXPECT_NE(v5, v4);

  EXPECT_THAT(v5.get<T5>(), IsOkAndHolds(deeply_nested));

  // tests maps with bytes key
  auto byte_key = unordered_map<Bytes, string>{{Bytes("foo"), "bar"},
                                               {Bytes("baz"), "qux"}};
  EXPECT_EQ(byte_key[Bytes("foo")], "bar");
  using T6 = decltype(byte_key);
  Value v6(byte_key);
  EXPECT_STATUS_OK(v6.get<T6>());
  EXPECT_THAT(v6.get<T5>(), Not(IsOk()));
  EXPECT_EQ(v6, v6);
  EXPECT_NE(v6, v5);
  auto retrieved_byte_key = v6.get<T6>();
  EXPECT_THAT(retrieved_byte_key, IsOkAndHolds(byte_key));
  EXPECT_EQ(byte_key[Bytes("foo")], (*retrieved_byte_key)[Bytes("foo")]);
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

TEST(Value, ProtoConversionMap) {
  using M = std::unordered_map<Bytes, std::int64_t>;
  auto data = M{{Bytes("foo"), 12}, {Bytes("bar"), 34}};
  Value const v(data);
  auto const p = bigtable_internal::ToProto(v);
  EXPECT_EQ(v, bigtable_internal::FromProto(p.first, p.second));
  EXPECT_TRUE(p.first.has_map_type());

  auto const& key_type = p.first.map_type().key_type();
  auto const& value_type = p.first.map_type().value_type();
  EXPECT_TRUE(key_type.has_bytes_type());
  EXPECT_TRUE(value_type.has_int64_type());

  Value const null_struct_value(MakeNullValue<M>());
  auto const null_struct_proto = bigtable_internal::ToProto(null_struct_value);
  EXPECT_TRUE(p.first.has_map_type());
}

TEST(Value, ProtoMapWithDuplicateKeys) {
  auto constexpr kTypeProto = R"""(
map_type {
  key_type {
    bytes_type {
    }
  }
  value_type {
    string_type {
    }
  }
}
)""";
  google::bigtable::v2::Type type_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(kTypeProto, &type_proto));

  auto constexpr kValueProto = R"""(
array_value {
  values {
    array_value {
      values {
        bytes_value: "foo"
      }
      values {
        string_value: "foo"
      }
    }
  }
  values {
    array_value {
      values {
        bytes_value: "foo"
      }
      values {
        string_value: "bar"
      }
    }
  }
}
)""";
  google::bigtable::v2::Value value_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(kValueProto, &value_proto));

  auto value = bigtable_internal::FromProto(type_proto, value_proto);
  auto map = value.get<std::unordered_map<Bytes, std::string>>();
  ASSERT_STATUS_OK(map);
  EXPECT_THAT(*map, ::testing::UnorderedElementsAre(
                        ::testing::Pair(Bytes("foo"), "bar")));
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

void SetProtoKind(Value& v,
                  std::unordered_map<std::string, std::int64_t> const& x) {
  auto p = bigtable_internal::ToProto(v);
  auto list = *p.second.mutable_array_value();
  for (auto&& e : x) {
    google::bigtable::v2::Value k;
    k.set_string_value(e.first);
    google::bigtable::v2::Value val;
    val.set_int_value(e.second);
    google::bigtable::v2::Value item;
    *(*item.mutable_array_value()).add_values() = k;
    *(*item.mutable_array_value()).add_values() = val;
    *list.add_values() = item;
  }
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
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

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
  EXPECT_THAT(v.get<std::tuple<double>>(), Not(IsOk()));
}

TEST(Value, GetBadMap) {
  using M = std::unordered_map<std::string, double>;
  Value v(M{{"foo", 12.34}, {"bar", 56.78}});
  ClearProtoKind(v);
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetNullProtoKind(v);
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(v, true);
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(v, "blah");
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(v, std::vector<int64_t>{1, 2});
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(v, std::make_tuple(1, 2));
  EXPECT_THAT(v.get<M>(), Not(IsOk()));

  SetProtoKind(
      v, std::unordered_map<std::string, int64_t>{{"foo", 12}, {"bar", 34}});
  EXPECT_THAT(v.get<M>(), Not(IsOk()));
}

struct OsModes {
  static std::ostream& normal(std::ostream& os) { return os; };
  static std::ostream& hex(std::ostream& os) { return os << std::hex; };
  static std::ostream& boolalpha(std::ostream& os) {
    return os << std::boolalpha;
  };
  static std::ostream& float4(std::ostream& os) {
    return os << std::showpoint << std::setprecision(4);
  };
  static std::ostream& alphahex(std::ostream& os) {
    return os << std::boolalpha << std::hex;
  };
};

TEST(Value, MapsWithValuesOutputStream) {
  struct TestCase {
    Value value;
    std::vector<std::string> expected;
    std::function<std::ostream&(std::ostream&)> manip;
  };

  std::vector<TestCase> test_case = {
      {Value(std::unordered_map<std::string, bool>{
           {{"bar", false}, {"foo", true}}}),
       {R"({"foo" : 1})", R"({"bar" : 0})"},
       OsModes::normal},
      {Value(std::unordered_map<std::string, bool>{
           {{"bar", false}, {"foo", true}}}),
       {R"({"foo" : true})", R"({"bar" : false})"},
       OsModes::boolalpha},
      {Value(std::unordered_map<std::string, std::int64_t>{
           {{"bar", 12}, {"foo", 34}}}),
       {R"({"foo" : 34})", R"({"bar" : 12})"},
       OsModes::normal},
      {Value(std::unordered_map<std::int64_t, std::int64_t>{
           {{10, 11}, {12, 13}}}),
       {R"({c : d})", R"({a : b})"},
       OsModes::hex},
      {Value(std::unordered_map<std::string, double>{
           {{"bar", 12.0}, {"foo", 34.0}}}),
       {R"({"foo" : 34})", R"({"bar" : 12})"},
       OsModes::normal},
      {Value(std::unordered_map<std::string, double>{
           {{"bar", 2.0}, {"foo", 3.0}}}),
       {R"({"foo" : 3.000})", R"({"bar" : 2.000})"},
       OsModes::float4},
      {Value(std::unordered_map<std::string, float>{
           {{"bar", 12.0F}, {"foo", 34.0F}}}),
       {R"({"foo" : 34})", R"({"bar" : 12})"},
       OsModes::normal},
      {Value(std::unordered_map<std::string, float>{
           {{"bar", 2.0F}, {"foo", 3.0F}}}),
       {R"({"foo" : 3.000})", R"({"bar" : 2.000})"},
       OsModes::float4},
      {Value(std::unordered_map<std::string, std::string>{
           {{"bar", std::string("a")}, {"foo", std::string("b")}}}),
       {R"({"foo" : "b"})", R"({"bar" : "a"})"},
       OsModes::normal},
      {Value(std::unordered_map<Bytes, Bytes>{
           {Bytes(std::string("bar")), Bytes(std::string("foo"))}}),
       {R"({B"bar" : B"foo"})"},
       OsModes::normal},
      {Value(std::unordered_map<Bytes, absl::CivilDay>{
           {Bytes(std::string("bar")), absl::CivilDay()}}),
       {R"({B"bar" : 1970-01-01})"},
       OsModes::normal},
      {Value(std::unordered_map<std::string, Timestamp>{{"bar", Timestamp()}}),
       {R"({"bar" : 1970-01-01T00:00:00Z})"},
       OsModes::normal},

      // Tests maps with null elements
      {Value(std::unordered_map<std::string, absl::optional<double>>{
           {"foo", 2.0}, {"bar", absl::optional<double>()}}),
       {R"({"bar" : NULL})", R"({"foo" : 2})"},
       OsModes::normal},

  };

  for (auto const& tc : test_case) {
    std::stringstream ss;
    tc.manip(ss) << tc.value;
    // 2 outer brackets and 2(n - 1) commas and spaces.
    size_t expected_length = 2 + (2 * (tc.expected.size() - 1));
    for (auto const& expected_kv : tc.expected) {
      EXPECT_TRUE(ss.str().find(expected_kv) != std::string::npos);
      expected_length += expected_kv.size();
    }
    EXPECT_EQ(ss.str().size(), expected_length);
  }
}

TEST(Value, OutputStream) {
  struct TestCase {
    Value value;
    std::string expected;
    std::function<std::ostream&(std::ostream&)> manip;
  };

  std::vector<TestCase> test_case = {
      {Value(false), "0", OsModes::normal},
      {Value(true), "1", OsModes::normal},
      {Value(false), "false", OsModes::boolalpha},
      {Value(true), "true", OsModes::boolalpha},
      {Value(42), "42", OsModes::normal},
      {Value(42), "2a", OsModes::hex},
      {Value(42.0), "42", OsModes::normal},
      {Value(42.0), "42.00", OsModes::float4},
      {Value(42.0F), "42", OsModes::normal},
      {Value(42.0F), "42.00", OsModes::float4},
      {Value(""), "", OsModes::normal},
      {Value("foo"), "foo", OsModes::normal},
      {Value("NULL"), "NULL", OsModes::normal},
      {Value(Bytes(std::string("DEADBEEF"))), R"(B"DEADBEEF")",
       OsModes::normal},
      {Value(Timestamp()), "1970-01-01T00:00:00Z", OsModes::normal},
      {Value(absl::CivilDay()), "1970-01-01", OsModes::normal},

      // Tests string quoting: No quotes for scalars; quotes within aggregates
      {Value(""), "", OsModes::normal},
      {Value("foo"), "foo", OsModes::normal},
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])",
       OsModes::normal},
      {Value(std::vector<std::string>{"\"a\"", "\"b\""}),
       R"(["\"a\"", "\"b\""])", OsModes::normal},
      {Value(std::unordered_map<std::string, std::string>{{"\"a\"", "\"b\""}}),
       R"({{"\"a\"" : "\"b\""}})", OsModes::normal},

      // Tests null values
      {MakeNullValue<bool>(), "NULL", OsModes::normal},
      {MakeNullValue<std::int64_t>(), "NULL", OsModes::normal},
      {MakeNullValue<double>(), "NULL", OsModes::normal},
      {MakeNullValue<float>(), "NULL", OsModes::normal},
      {MakeNullValue<std::string>(), "NULL", OsModes::normal},
      {MakeNullValue<Bytes>(), "NULL", OsModes::normal},
      {MakeNullValue<Timestamp>(), "NULL", OsModes::normal},
      {MakeNullValue<absl::CivilDay>(), "NULL", OsModes::normal},

      // Tests arrays
      {Value(std::vector<bool>{false, true}), "[0, 1]", OsModes::normal},
      {Value(std::vector<bool>{false, true}), "[false, true]",
       OsModes::boolalpha},
      {Value(std::vector<std::int64_t>{10, 11}), "[10, 11]", OsModes::normal},
      {Value(std::vector<std::int64_t>{10, 11}), "[a, b]", OsModes::hex},
      {Value(std::vector<double>{1.0, 2.0}), "[1, 2]", OsModes::normal},
      {Value(std::vector<double>{1.0, 2.0}), "[1.000, 2.000]", OsModes::float4},
      {Value(std::vector<float>{1.0F, 2.0F}), "[1, 2]", OsModes::normal},
      {Value(std::vector<float>{1.0F, 2.0F}), "[1.000, 2.000]",
       OsModes::float4},
      {Value(std::vector<std::string>{"a", "b"}), R"(["a", "b"])",
       OsModes::normal},
      {Value(std::vector<Bytes>{2}), R"([B"", B""])", OsModes::normal},
      {Value(std::vector<absl::CivilDay>{2}), "[1970-01-01, 1970-01-01]",
       OsModes::normal},
      {Value(std::vector<Timestamp>{1}), "[1970-01-01T00:00:00Z]",
       OsModes::normal},

      // Tests arrays with null elements
      {Value(std::vector<absl::optional<double>>{1, {}, 2}), "[1, NULL, 2]",
       OsModes::normal},

      // Tests null arrays
      {MakeNullValue<std::vector<bool>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<std::int64_t>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<double>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<float>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<std::string>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<Bytes>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<absl::CivilDay>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::vector<Timestamp>>(), "NULL", OsModes::normal},

      // Tests structs
      {Value(std::make_tuple(true, 123)), "(1, 123)", OsModes::normal},
      {Value(std::make_tuple(true, 123)), "(true, 7b)", OsModes::alphahex},
      {Value(std::make_tuple(std::make_pair("A", true),
                             std::make_pair("B", 123))),
       R"(("A": 1, "B": 123))", OsModes::normal},
      {Value(std::make_tuple(std::make_pair("A", true),
                             std::make_pair("B", 123))),
       R"(("A": true, "B": 7b))", OsModes::alphahex},
      {Value(std::make_tuple(
           std::vector<std::int64_t>{10, 11, 12},
           std::make_pair("B", std::vector<std::int64_t>{13, 14, 15}))),
       R"(([10, 11, 12], "B": [13, 14, 15]))", OsModes::normal},
      {Value(std::make_tuple(
           std::vector<std::int64_t>{10, 11, 12},
           std::make_pair("B", std::vector<std::int64_t>{13, 14, 15}))),
       R"(([a, b, c], "B": [d, e, f]))", OsModes::hex},
      {Value(std::make_tuple(std::make_tuple(
           std::make_tuple(std::vector<std::int64_t>{10, 11, 12})))),
       "((([10, 11, 12])))", OsModes::normal},
      {Value(std::make_tuple(std::make_tuple(
           std::make_tuple(std::vector<std::int64_t>{10, 11, 12})))),
       "((([a, b, c])))", OsModes::hex},

      // Tests struct with null members
      {Value(std::make_tuple(absl::optional<bool>{})), "(NULL)",
       OsModes::normal},
      {Value(std::make_tuple(absl::optional<bool>{}, 123)), "(NULL, 123)",
       OsModes::normal},
      {Value(std::make_tuple(absl::optional<bool>{}, 123)), "(NULL, 7b)",
       OsModes::hex},
      {Value(std::make_tuple(absl::optional<bool>{},
                             absl::optional<std::int64_t>{})),
       "(NULL, NULL)", OsModes::normal},

      // Tests null structs
      {MakeNullValue<std::tuple<bool>>(), "NULL", OsModes::normal},
      {MakeNullValue<std::tuple<bool, std::int64_t>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::tuple<float, std::string>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::tuple<double, Bytes, Timestamp>>(), "NULL",
       OsModes::normal},

      // Tests null maps
      {MakeNullValue<std::unordered_map<std::int64_t, bool>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<std::int64_t, std::int64_t>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<std::int64_t, double>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<std::int64_t, float>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<Bytes, std::string>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<Bytes, Bytes>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<Bytes, absl::CivilDay>>(), "NULL",
       OsModes::normal},
      {MakeNullValue<std::unordered_map<Bytes, Timestamp>>(), "NULL",
       OsModes::normal}};

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

  // std::unordered_map<...>
  // Not included, because a raw map cannot be streamed.
}

void TestTypeAndValuesMatch(std::string const& type_text,
                            std::string const& value_text, bool expected) {
  google::bigtable::v2::Type type;
  ASSERT_TRUE(TextFormat::ParseFromString(type_text, &type));
  google::bigtable::v2::Value value;
  ASSERT_TRUE(TextFormat::ParseFromString(value_text, &value));
  auto result = Value::TypeAndValuesMatch(type, value);
  if (expected) {
    EXPECT_STATUS_OK(result);
  } else {
    EXPECT_THAT(result, Not(IsOk()));
  }
}

TEST(Value, TypeAndValuesMatchScalar) {
  TestTypeAndValuesMatch("int64_type {}", "int_value: 123", true);
  TestTypeAndValuesMatch("string_type {}", "string_value: 'hello'", true);
  TestTypeAndValuesMatch("bool_type {}", "bool_value: true", true);
  TestTypeAndValuesMatch("float64_type {}", "float_value: 3.14", true);
  TestTypeAndValuesMatch("float32_type {}", "float_value: 3.14", true);
  TestTypeAndValuesMatch("bytes_type {}", "bytes_value: 'bytes'", true);
  TestTypeAndValuesMatch("timestamp_type {}",
                         "timestamp_value: { seconds: 123 }", true);
  TestTypeAndValuesMatch("date_type {}",
                         "date_value: { year: 2025, month: 1, day: 1 }", true);
}

TEST(Value, TypeAndValuesMatchScalarMismatch) {
  TestTypeAndValuesMatch("int64_type {}", "string_value: 'mismatch'", false);
  TestTypeAndValuesMatch("string_type {}", "int_value: 123", false);
}

TEST(Value, TypeAndValuesMatchNullScalar) {
  TestTypeAndValuesMatch("int64_type {}", "", true);
  TestTypeAndValuesMatch("string_type {}", "", true);
}

TEST(Value, TypeAndValuesMatchArray) {
  auto const* type = R"pb(
    array_type { element_type { int64_type {} } }
  )pb";
  auto const* matching_value = R"pb(
    array_value {
      values { int_value: 1 }
      values { int_value: 2 }
    }
  )pb";
  TestTypeAndValuesMatch(type, matching_value, true);
}

TEST(Value, TypeAndValuesMatchArrayMismatchElementType) {
  auto const* type = R"pb(
    array_type { element_type { int64_type {} } }
  )pb";
  auto const* mismatched_value = R"pb(
    array_value {
      values { int_value: 1 }
      values { string_value: "2" }
    }
  )pb";
  TestTypeAndValuesMatch(type, mismatched_value, false);
}

TEST(Value, TypeAndValuesMatchArrayMismatchScalar) {
  auto const* type = R"pb(
    array_type { element_type { int64_type {} } }
  )pb";
  TestTypeAndValuesMatch(type, "int_value: 123", false);
}

TEST(Value, TypeAndValuesMatchArrayWithNull) {
  auto const* type = R"pb(
    array_type { element_type { int64_type {} } }
  )pb";
  auto const* value_with_null = R"pb(
    array_value {
      values { int_value: 1 }
      values {}  # null
      values { int_value: 3 }
    }
  )pb";
  TestTypeAndValuesMatch(type, value_with_null, true);
}

TEST(Value, TypeAndValuesMatchStruct) {
  auto const* type = R"pb(
    struct_type {
      fields {
        field_name: "name"
        type { string_type {} }
      }
      fields {
        field_name: "age"
        type { int64_type {} }
      }
    }
  )pb";
  auto const* matching_value = R"pb(
    array_value {
      values { string_value: "John" }
      values { int_value: 42 }
    }
  )pb";
  TestTypeAndValuesMatch(type, matching_value, true);
}

TEST(Value, TypeAndValuesMatchStructMismatchFieldType) {
  auto const* type = R"pb(
    struct_type {
      fields {
        field_name: "name"
        type { string_type {} }
      }
      fields {
        field_name: "age"
        type { int64_type {} }
      }
    }
  )pb";
  auto const* mismatched_value = R"pb(
    array_value {
      values { string_value: "John" }
      values { string_value: "42" }
    }
  )pb";
  TestTypeAndValuesMatch(type, mismatched_value, false);
}

TEST(Value, TypeAndValuesMatchStructMismatchFieldCount) {
  auto const* type = R"pb(
    struct_type {
      fields { type { string_type {} } }
      fields { type { int64_type {} } }
    }
  )pb";
  auto const* mismatched_value = R"pb(
    array_value { values { string_value: "John" } }
  )pb";
  // The current implementation has a bug and will return true here.
  // This test will fail until the bug is fixed.
  TestTypeAndValuesMatch(type, mismatched_value, false);
}

TEST(Value, TypeAndValuesMatchStructMismatchScalar) {
  auto const* type = R"pb(
    struct_type { fields { type { string_type {} } } }
  )pb";
  TestTypeAndValuesMatch(type, "string_value: 'John'", false);
}

TEST(Value, TypeAndValuesMatchStructWithNull) {
  auto const* type = R"pb(
    struct_type {
      fields { type { string_type {} } }
      fields { type { int64_type {} } }
    }
  )pb";
  auto const* value_with_null = R"pb(
    array_value {
      values { string_value: "John" }
      values {}
    }
  )pb";
  TestTypeAndValuesMatch(type, value_with_null, true);
}

TEST(Value, TypeAndValuesMatchMap) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  auto const* matching_value = R"pb(
    array_value {
      values {
        array_value {
          values { string_value: "key1" }
          values { int_value: 1 }
        }
      }
    }
  )pb";
  TestTypeAndValuesMatch(type, matching_value, true);
}

TEST(Value, TypeAndValuesMatchMapMismatchKeyType) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  auto const* mismatched_value = R"pb(
    array_value {
      values {
        array_value {
          values { int_value: 1 }
          values { int_value: 1 }
        }
      }
    }
  )pb";
  TestTypeAndValuesMatch(type, mismatched_value, false);
}

TEST(Value, TypeAndValuesMatchMapMismatchValueType) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  auto const* mismatched_value = R"pb(
    array_value {
      values {
        array_value {
          values { string_value: "key1" }
          values { string_value: "1" }
        }
      }
    }
  )pb";
  TestTypeAndValuesMatch(type, mismatched_value, false);
}

TEST(Value, TypeAndValuesMatchMapMismatchScalar) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  TestTypeAndValuesMatch(type, "string_value: 'foo'", false);
}

TEST(Value, TypeAndValuesMatchMapMalformedEntry) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  auto const* malformed_value = R"pb(
    array_value { values { array_value { values { string_value: "key1" } } } }
  )pb";
  TestTypeAndValuesMatch(type, malformed_value, false);
}

TEST(Value, TypeAndValuesMatchMapWithNullValue) {
  auto const* type = R"pb(
    map_type {
      key_type { string_type {} }
      value_type { int64_type {} }
    }
  )pb";
  auto const* value_with_null = R"pb(
    array_value {
      values {
        array_value {
          values { string_value: "key1" }
          values {}
        }
      }
    }
  )pb";
  TestTypeAndValuesMatch(type, value_with_null, true);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
