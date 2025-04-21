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

#include "google/cloud/internal/http_header.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;

TEST(HttpHeader, ConstructionAndStringFormatting) {
  HttpHeader empty;
  EXPECT_THAT(std::string(empty), Eq(""));

  HttpHeader no_value("key");
  EXPECT_THAT(std::string(no_value), Eq("key:"));

  HttpHeader single_value("key", "value");
  EXPECT_THAT(std::string(single_value), Eq("key: value"));

  HttpHeader multi_value("key", std::vector<std::string>{"value1", "value2"});
  EXPECT_THAT(std::string(multi_value), Eq("key: value1,value2"));
  HttpHeader multi_literal_value("key", {"value1", "value2"});
  EXPECT_THAT(std::string(multi_literal_value), Eq("key: value1,value2"));
}

TEST(HttpHeader, Equality) {
  HttpHeader empty;
  // Key field tests
  EXPECT_TRUE(empty == empty);
  EXPECT_FALSE(empty != empty);
  EXPECT_FALSE(empty == HttpHeader("key"));
  EXPECT_FALSE(HttpHeader("key") == empty);
  EXPECT_TRUE(HttpHeader("Key") == HttpHeader("key"));
  EXPECT_TRUE(HttpHeader("key") == HttpHeader("Key"));
  EXPECT_TRUE(HttpHeader("Content-Length") == HttpHeader("content-length"));

  // Values field tests
  EXPECT_TRUE(HttpHeader("key", "value") == HttpHeader("key", "value"));
  EXPECT_FALSE(HttpHeader("key", "value") == HttpHeader("key", "Value"));
  EXPECT_FALSE(HttpHeader("key", {"v1", "v2"}) == HttpHeader("Key", "v1"));
  EXPECT_FALSE(HttpHeader("key", {"v1", "v2"}) == HttpHeader("Key", {"v1"}));
  EXPECT_FALSE(HttpHeader("key", {"v1", "v2"}) ==
               HttpHeader("Key", {"v1", "V2"}));
  EXPECT_FALSE(HttpHeader("key", {"V1", "v2"}) ==
               HttpHeader("Key", {"v1", "V2"}));
  EXPECT_TRUE(HttpHeader("key", {"v1", "v2"}) ==
              HttpHeader("Key", {"v1", "v2"}));
  EXPECT_FALSE(HttpHeader("key", {"v1", "v2"}) ==
               HttpHeader("Key", {"v2", "v1"}));
}

TEST(HttpHeader, LessThan) {
  EXPECT_TRUE(HttpHeader("hey") < HttpHeader("key"));
  EXPECT_FALSE(HttpHeader("key") < HttpHeader("key"));
  EXPECT_FALSE(HttpHeader("key") < HttpHeader("hey"));
  EXPECT_TRUE(HttpHeader("Hey") < HttpHeader("key"));
  EXPECT_FALSE(HttpHeader("key") < HttpHeader("Key"));
  EXPECT_FALSE(HttpHeader("key") < HttpHeader("Hey"));
}

TEST(HttpHeader, IsSameKey) {
  EXPECT_TRUE(HttpHeader("key").IsSameKey("key"));
  EXPECT_TRUE(HttpHeader("Key").IsSameKey("key"));
  EXPECT_TRUE(HttpHeader("Key").IsSameKey("Key"));
  EXPECT_FALSE(HttpHeader("Key").IsSameKey("ey"));

  EXPECT_TRUE(HttpHeader("key").IsSameKey(HttpHeader("key")));
  EXPECT_TRUE(HttpHeader("Key").IsSameKey(HttpHeader("key")));
  EXPECT_TRUE(HttpHeader("Key").IsSameKey(HttpHeader("Key")));
  EXPECT_FALSE(HttpHeader("Key").IsSameKey(HttpHeader("ey")));
}

TEST(HttpHeader, DebugString) {
  HttpHeader empty;
  EXPECT_THAT(empty.DebugString(), Eq(""));

  HttpHeader no_value("key");
  EXPECT_THAT(no_value.DebugString(), Eq("key:"));

  HttpHeader short_value("key", "short");
  EXPECT_THAT(short_value.DebugString(), Eq("key: short"));

  HttpHeader long_value("key", "valuelongerthantruncatelength");
  EXPECT_THAT(long_value.DebugString(), Eq("key: valuelonge"));
}

TEST(HttpHeader, MergeHeader) {
  HttpHeader k1_v1("k1", "k1-value1");
  HttpHeader k2_v1("k2", "k2-value1");
  EXPECT_THAT(k1_v1.MergeHeader(k2_v1), Eq(HttpHeader("k1", "k1-value1")));
  EXPECT_THAT(k1_v1.MergeHeader(std::move(k2_v1)),
              Eq(HttpHeader("k1", "k1-value1")));

  HttpHeader k1_v2("k1", "k1-value2");
  EXPECT_THAT(k1_v1.MergeHeader(k1_v2),
              Eq(HttpHeader("k1", {"k1-value1", "k1-value2"})));
  EXPECT_THAT(k1_v2, Eq(HttpHeader("k1", "k1-value2")));
  k1_v1 = HttpHeader("k1", "k1-value1");
  EXPECT_THAT(k1_v1.MergeHeader(std::move(k1_v2)),
              Eq(HttpHeader("k1", {"k1-value1", "k1-value2"})));

  HttpHeader k1_v3("k1", "k1-value3");
  k1_v1 = HttpHeader("k1", {"k1-value1"});
  EXPECT_THAT(k1_v3.MergeHeader(k1_v1),
              Eq(HttpHeader("k1", {"k1-value3", "k1-value1"})));
  k1_v3 = HttpHeader("k1", "k1-value3");
  EXPECT_THAT(k1_v3.MergeHeader(std::move(k1_v1)),
              Eq(HttpHeader("k1", {"k1-value3", "k1-value1"})));

  HttpHeader k3_v1("k3", "k3-value10");
  HttpHeader k3_v5("k3", "k3-value5");
  EXPECT_THAT(k3_v1.MergeHeader(HttpHeader("k3", {"k3-value2", "k3-value3"}))
                  .MergeHeader(k3_v5)
                  .MergeHeader((std::move(k3_v5))),
              Eq(HttpHeader("k3", {"k3-value10", "k3-value2", "k3-value3",
                                   "k3-value5", "k3-value5"})));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
