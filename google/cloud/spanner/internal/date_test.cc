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

#include "google/cloud/spanner/internal/date.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

TEST(Date, DateToString) {
  EXPECT_EQ("2019-06-21", DateToString(Date(2019, 6, 21)));
  EXPECT_EQ("1066-10-14", DateToString(Date(1066, 10, 14)));
  EXPECT_EQ("0865-03-21", DateToString(Date(865, 3, 21)));
  EXPECT_EQ("0014-08-19", DateToString(Date(14, 8, 19)));
}

TEST(Date, DateFromString) {
  EXPECT_EQ(Date(2019, 6, 21), DateFromString("2019-06-21").value());
}

TEST(Date, DateFromStringFailure) {
  EXPECT_FALSE(DateFromString(""));
  EXPECT_FALSE(DateFromString("garbage in"));
  EXPECT_FALSE(DateFromString("2018-13-02"));
  EXPECT_FALSE(DateFromString("2019-06-31"));
  EXPECT_FALSE(DateFromString("2019-06-21x"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
