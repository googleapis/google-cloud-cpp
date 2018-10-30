// Copyright 2018 Google LLC
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

#include "google/cloud/internal/invoke_result.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {
TEST(ResultOfTest, Lambda) {
  // This test mostly uses static assertions because the result_of classes are
  // largely meta-functions.

  auto l = [](long, int) -> int { return 7; };

  using F = decltype(l);

  using R = invoke_result<F, long, int>::type;

  static_assert(std::is_same<R, int>::value,
                "expected `int` in invoke_result_t<>");
}

std::string test_function(int, std::string const&) { return "42"; }

TEST(ResultOfTest, Function) {
  // This test mostly uses static assertions because the result_of classes are
  // largely meta-functions.

  using R1 = decltype(invoker_function(test_function, 7, std::string{}));
  static_assert(std::is_same<R1, std::string>::value,
                "expected `std::string` in R1==decltype(invoker_function())");

  using R2 = decltype(invoker_function(test_function, std::declval<int>(),
                                       std::declval<std::string const&>()));
  static_assert(std::is_same<R2, std::string>::value,
                "expected `std::string` in R2==decltype(invoker_function())");
  EXPECT_TRUE((std::is_same<R2, std::string>::value));

  using F = decltype(test_function);
  using R3 = decltype(invoker_function(std::declval<F>(), std::declval<int>(),
                                       std::declval<std::string const&>()));
  static_assert(std::is_same<R3, std::string>::value,
                "expected `std::string` in R3==decltype(invoker_function())");
  EXPECT_TRUE((std::is_same<R3, std::string>::value));

  using R4 = invoke_result<F, int, std::string const&>;
  static_assert(std::is_same<R4::type, std::string>::value,
                "expected `std::string` in R4==invoke_result_t<>");

  static_assert(
      is_invocable<F, int, std::string const&>::value,
      "expected `is_invocable<F, int, std::string const&>` to be true");

  static_assert(is_invocable<F, int, std::string>::value,
                "expected `is_invocable<F, int, std::string>` to be true");

  static_assert(is_invocable<F, int, std::string&>::value,
                "expected `is_invocable<F, int, std::string&>` to be true");

  static_assert(not is_invocable<F, std::string>::value,
                "expected `is_invocable<F, std::string>` to be false");

  static_assert(not is_invocable<F>::value,
                "expected `is_invocable<F>` to be false");

  // Some compilers create warnings/errors if the function is not used, even
  // though in this test we are mostly interested in its type.
  EXPECT_EQ("42", test_function(7, "7"));
}

struct TestStruct {
  void DoSomething(std::string const&, int) {}
  template <typename F>
  void DoSomethingTemplated(std::string const&, F&&) {}
};

TEST(ResultOfTest, TestMemberFn) {
  using DoSomethingType = decltype(&TestStruct::DoSomething);
  static_assert(is_invocable<DoSomethingType, TestStruct&, std::string const&,
                             int>::value,
                "expected `is_invocable<DoSomethingType, TestStruct&, "
                "std::string const&, int>` to be true");
  using DoSomethingTemplatedType =
      decltype(&TestStruct::DoSomethingTemplated<std::string>);
  static_assert(is_invocable<DoSomethingTemplatedType, TestStruct&,
                             std::string const&, std::string&&>::value,
                "expected `is_invocable<DoSomethingTemplatedType, TestStruct&, "
                "std::string const&, int>` to be true");
  using DoSomethingType = decltype(&TestStruct::DoSomething);
  static_assert(not is_invocable<DoSomethingType, TestStruct&, int, int>::value,
                "expected `is_invocable<DoSomethingType, TestStruct&, "
                "std::string const&, int>` to be true");
  using DoSomethingTemplatedType =
      decltype(&TestStruct::DoSomethingTemplated<std::string>);
  static_assert(not is_invocable<DoSomethingTemplatedType, TestStruct&, int,
                                 std::string&&>::value,
                "expected `is_invocable<DoSomethingTemplatedType, TestStruct&, "
                "std::string const&, int>` to be true");
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
