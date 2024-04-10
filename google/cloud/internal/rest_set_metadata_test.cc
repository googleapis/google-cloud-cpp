// Copyright 2024 Google LLC
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

#include "google/cloud/internal/rest_set_metadata.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/rest_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(RestContextTest, SetMetadataBase) {
  RestContext lhs;
  SetMetadata(lhs, Options{}, {}, "api-client-header");
  EXPECT_THAT(lhs.headers(),
              UnorderedElementsAre(
                  Pair("x-goog-api-client", ElementsAre("api-client-header"))));
}

TEST(RestContextTest, SetMetadataSingleParam) {
  RestContext lhs;
  SetMetadata(lhs, Options{}, {"p1=v1"}, "api-client-header");
  EXPECT_THAT(lhs.headers(),
              UnorderedElementsAre(
                  Pair("x-goog-api-client", ElementsAre("api-client-header")),
                  Pair("x-goog-request-params", ElementsAre("p1=v1"))));
}

TEST(RestContextTest, SetMetadataFull) {
  RestContext lhs;
  SetMetadata(lhs,
              Options{}
                  .set<UserProjectOption>("user-project")
                  .set<QuotaUserOption>("quota-user")
                  .set<FieldMaskOption>("items.name,token")
                  .set<ServerTimeoutOption>(std::chrono::milliseconds(1050))
                  .set<CustomHeadersOption>(
                      {{"custom-header-1", "v1"}, {"custom-header-2", "v2"}}),
              {"p1=v1", "p2=v2"}, "api-client-header");
  EXPECT_THAT(lhs.headers(),
              UnorderedElementsAre(
                  Pair("x-goog-api-client", ElementsAre("api-client-header")),
                  Pair("x-goog-request-params", ElementsAre("p1=v1&p2=v2")),
                  Pair("x-goog-user-project", ElementsAre("user-project")),
                  Pair("x-goog-quota-user", ElementsAre("quota-user")),
                  Pair("x-goog-fieldmask", ElementsAre("items.name,token")),
                  Pair("x-server-timeout", ElementsAre("1.050")),
                  Pair("custom-header-1", ElementsAre("v1")),
                  Pair("custom-header-2", ElementsAre("v2"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
