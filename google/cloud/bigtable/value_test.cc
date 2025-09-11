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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Not;

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
  // TODO: enable Proto equality
  EXPECT_THAT(null_protos.first, IsProtoEqual(protos.first));
  EXPECT_EQ(null_protos.second.null_value(),
            google::protobuf::NullValue::NULL_VALUE);

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
    // TODO: enable vector support
    // TestBasicSemantics(std::vector<bool>(5, x));
    // std::vector<absl::optional<bool>> v(5, x);
    // v.resize(10);
    // TestBasicSemantics(v);
  }
}

TEST(Value, Equality) {
  std::vector<std::pair<Value, Value>> test_cases = {
      {Value(false), Value(true)},
  };

  for (auto const& tc : test_cases) {
    EXPECT_EQ(tc.first, tc.first);
    EXPECT_EQ(tc.second, tc.second);
    EXPECT_NE(tc.first, tc.second);
    // Compares tc.first to tc2.second, which ensures that different "kinds" of
    // value are never equal.
    for (auto const& tc2 : test_cases) {
      // TODO: since we only support bool, this is a no-op for now
      EXPECT_NE(tc.first, tc2.second);
    }
  }
}

TEST(Value, ConstructionFromLiterals) {
  Value v_bool(true);
  EXPECT_EQ(true, *v_bool.get<bool>());
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

void SetProtoKind(Value& v, google::protobuf::NullValue x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_null_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, double x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_number_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, float x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_number_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void SetProtoKind(Value& v, char const* x) {
  auto p = bigtable_internal::ToProto(v);
  p.second.set_string_value(x);
  v = bigtable_internal::FromProto(p.first, p.second);
}

void ClearProtoKind(Value& v) {
  auto p = bigtable_internal::ToProto(v);
  p.second.clear_kind();
  v = bigtable_internal::FromProto(p.first, p.second);
}

TEST(Value, GetBadBool) {
  Value v(true);
  ClearProtoKind(v);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, google::protobuf::NULL_VALUE);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, 0.0);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, 0.0F);
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));

  SetProtoKind(v, "hello");
  EXPECT_THAT(v.get<bool>(), Not(IsOk()));
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
