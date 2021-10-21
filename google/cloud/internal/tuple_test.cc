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

#include "google/cloud/internal/tuple.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

TEST(ApplyTest, Simple) {
  std::string s;
  int i;
  char c;
  auto res = ::google::cloud::internal::apply(
      [&](std::string new_s, int new_i, char new_c) {
        s = std::move(new_s);
        i = new_i;
        c = new_c;
        return i * 2;
      },
      std::make_tuple("hello world", 42, 'x'));
  static_assert(std::is_same<decltype(res), int>::value, "");
  EXPECT_EQ("hello world", s);
  EXPECT_EQ(42, i);
  EXPECT_EQ('x', c);
  EXPECT_EQ(84, res);
}

TEST(ApplyTest, VoidResult) {
  int i;
  auto f = [&](int new_i) { i = new_i; };
  static_assert(
      std::is_same<void,
                   invoke_result_t<decltype(::google::cloud::internal::apply<
                                            decltype(f), std::tuple<int>>),
                                   decltype(f), std::tuple<int>>>::value,
      "");
  ::google::cloud::internal::apply(f, std::make_tuple(42));
  EXPECT_EQ(42, i);
}

TEST(ApplyTest, NoArgs) {
  int i = ::google::cloud::internal::apply([] { return 42; }, std::tuple<>());
  EXPECT_EQ(42, i);
}

TEST(ApplyTest, TupleByReference) {
  std::string s;
  auto tuple = std::make_tuple("hello world");
  auto res = ::google::cloud::internal::apply(
      [&](std::string new_s) {
        s = std::move(new_s);
        return s.size();
      },
      tuple);
  static_assert(std::is_same<decltype(res), std::size_t>::value, "");
  EXPECT_EQ("hello world", s);
  EXPECT_EQ(11, res);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
