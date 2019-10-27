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

#include "google/cloud/storage/internal/tuple_filter.h"
#include "google/cloud/internal/make_unique.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

TEST(TupleFilter, EmptyTuple) {
  auto res = StaticTupleFilter<std::is_integral>(std::tuple<>());
  static_assert(std::tuple_size<decltype(res)>::value == 0, "");
}

TEST(TupleFilter, FullMatch) {
  auto res = StaticTupleFilter<std::is_integral>(
      std::tuple<int, short, long>(1, 2, 3));
  static_assert(std::tuple_size<decltype(res)>::value == 3, "");
  auto i1 = std::get<0>(res);
  auto i2 = std::get<1>(res);
  auto i3 = std::get<2>(res);
  static_assert(std::is_same<decltype(i1), int>::value, "");
  static_assert(std::is_same<decltype(i2), short>::value, "");
  static_assert(std::is_same<decltype(i3), long>::value, "");
  EXPECT_EQ(1, std::get<0>(res));
  EXPECT_EQ(2, std::get<1>(res));
  EXPECT_EQ(3, std::get<2>(res));
}

TEST(TupleFilter, NoMatch) {
  auto res =
      StaticTupleFilter<std::is_pointer>(std::tuple<int, short, long>(1, 2, 3));
  static_assert(std::tuple_size<decltype(res)>::value == 0, "");
}

TEST(TupleFilter, Selective) {
  auto res = StaticTupleFilter<NotAmong<long, short>::TPred>(
      std::tuple<int, std::string, short>(5, "asd", 7));
  static_assert(std::tuple_size<decltype(res)>::value == 2, "");
  auto i1 = std::get<0>(res);
  auto i2 = std::get<1>(res);
  static_assert(std::is_same<decltype(i1), int>::value, "");
  static_assert(std::is_same<decltype(i2), std::string>::value, "");
  EXPECT_EQ(5, std::get<0>(res));
  EXPECT_EQ("asd", std::get<1>(res));
}

// Test that forwarding rvalues works.
TEST(TupleFilter, NonCopyable) {
  std::unique_ptr<int> iptr = google::cloud::internal::make_unique<int>(42);
  auto res = std::get<0>(StaticTupleFilter<NotAmong<long>::TPred>(
      std::tuple<std::unique_ptr<int>>(std::move(iptr))));
  EXPECT_EQ(42, *res);
  static_assert(std::is_same<std::unique_ptr<int>, decltype(res)>::value, "");
}

// Test that forwarding references works.
TEST(TupleFilter, ByReference) {
  std::unique_ptr<int> iptr = google::cloud::internal::make_unique<int>(42);
  // This wouldn't work because get<0> returns a std::unique_ptr<int>&:
  // auto res = std::get<0>(StaticTupleFilter<NotAmong<long>::TPred>(
  //     std::tie(iptr)));
  auto& res =
      std::get<0>(StaticTupleFilter<NotAmong<long>::TPred>(std::tie(iptr)));
  // res is only an alias to iptr
  EXPECT_EQ(&res, &iptr);
  EXPECT_EQ(42, *res);
}

// Test that forwarding references works.
TEST(TupleFilter, TupleByReference) {
  std::unique_ptr<int> iptr = google::cloud::internal::make_unique<int>(42);
  auto t = std::tie(iptr);
  // This wouldn't work because get<0> returns a std::unique_ptr<int>&:
  // auto res = std::get<0>(StaticTupleFilter<NotAmong<long>::TPred>(
  //     std::tie(iptr)));
  auto& res = std::get<0>(StaticTupleFilter<NotAmong<long>::TPred>(t));
  // res is only an alias to iptr
  EXPECT_EQ(&res, &iptr);
  EXPECT_EQ(42, *res);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
