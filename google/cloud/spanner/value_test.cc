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
#include <gmock/gmock.h>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
template <typename T>
void TestBasicSemantics(T init) {
  Value const v{init};

  EXPECT_TRUE(v.is<T>());
  EXPECT_FALSE(v.is_null());

  EXPECT_EQ(init, *v.get<T>());
  EXPECT_EQ(init, static_cast<T>(v));

  Value const copy = v;
  EXPECT_EQ(copy, v);

  Value const moved = std::move(copy);
  EXPECT_EQ(moved, v);

  // Tests a null Value of type `T`.
  Value const null = Value::MakeNull<T>();
  EXPECT_TRUE(null.is<T>());
  EXPECT_TRUE(null.is_null());
  EXPECT_FALSE(null.get<T>().ok());
  EXPECT_NE(null, v);
}

TEST(Value, BasicSemantics) {
  for (auto x : {false, true}) {
    SCOPED_TRACE("Testing: bool " + std::to_string(x));
    TestBasicSemantics(x);
  }

  auto const min64 = std::numeric_limits<std::int64_t>::min();
  auto const max64 = std::numeric_limits<std::int64_t>::max();
  for (auto x : std::vector<std::int64_t>{min64, -1, 0, 1, max64}) {
    SCOPED_TRACE("Testing: std::int64_t " + std::to_string(x));
    TestBasicSemantics(x);
  }

  // Note: We skip testing the NaN case here because NaN always compares not
  // equal, even with itself. So NaN is handled in a separate test.
  auto const inf = std::numeric_limits<double>::infinity();
  for (auto x : {-inf, -1.0, -0.5, 0.0, 0.5, 1.0, inf}) {
    SCOPED_TRACE("Testing: double " + std::to_string(x));
    TestBasicSemantics(x);
  }

  for (auto x : std::vector<std::string>{"", "f", "foo", "12345678901234567"}) {
    SCOPED_TRACE("Testing: std::string " + std::string(x));
    TestBasicSemantics(x);
  }
}

TEST(Value, DoubleNaN) {
  double const nan = std::nan("NaN");
  Value v{nan};
  EXPECT_TRUE(v.is<double>());
  EXPECT_FALSE(v.is_null());
  EXPECT_TRUE(std::isnan(*v.get<double>()));

  // Since IEEE 754 defines that nan is not equal to itself, then a Value with
  // NaN should not be equal to itself.
  EXPECT_NE(nan, nan);
  EXPECT_NE(v, v);
}

TEST(Value, MixingTypes) {
  using A = bool;
  using B = std::int64_t;

  Value a(A{});
  EXPECT_FALSE(a.is_null());
  EXPECT_TRUE(a.is<A>());
  EXPECT_TRUE(a.get<A>().ok());
  EXPECT_FALSE(a.is<B>());
  EXPECT_FALSE(a.get<B>().ok());

  Value null_a = Value::MakeNull<A>();
  EXPECT_TRUE(null_a.is_null());
  EXPECT_TRUE(null_a.is<A>());
  EXPECT_FALSE(null_a.get<A>().ok());
  EXPECT_FALSE(null_a.is<B>());
  EXPECT_FALSE(null_a.get<B>().ok());

  EXPECT_NE(null_a, a);

  Value b(B{});
  EXPECT_FALSE(b.is_null());
  EXPECT_TRUE(b.is<B>());
  EXPECT_TRUE(b.get<B>().ok());
  EXPECT_FALSE(b.is<A>());
  EXPECT_FALSE(b.get<A>().ok());

  EXPECT_NE(b, a);
  EXPECT_NE(b, null_a);

  Value null_b = Value::MakeNull<B>();
  EXPECT_TRUE(null_b.is_null());
  EXPECT_TRUE(null_b.is<B>());
  EXPECT_FALSE(null_b.get<B>().ok());
  EXPECT_FALSE(null_b.is<A>());
  EXPECT_FALSE(null_b.get<A>().ok());

  EXPECT_NE(null_b, b);
  EXPECT_NE(null_b, null_a);
  EXPECT_NE(null_b, a);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
