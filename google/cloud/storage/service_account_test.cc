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

#include "google/cloud/storage/service_account.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/internal/service_account_requests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

ServiceAccount CreateServiceAccountForTest() {
  return internal::ServiceAccountParser::FromString(R"""({
      "email_address": "service-123@example.com",
      "kind": "storage#serviceAccount"
})""")
      .value();
}

/// @test Verify that we parse JSON objects into ServiceAccount objects.
TEST(ServiceAccountTest, Parse) {
  auto actual = CreateServiceAccountForTest();

  EXPECT_EQ("service-123@example.com", actual.email_address());
  EXPECT_EQ("storage#serviceAccount", actual.kind());
}

/// @test Verify that we parse JSON objects into ServiceAccount objects.
TEST(ServiceAccountTest, ParseFailure) {
  auto actual = internal::ServiceAccountParser::FromString("{123");
  EXPECT_THAT(actual, Not(IsOk()));
}

/// @test Verify that the IOStream operator works as expected.
TEST(ServiceAccountTest, IOStream) {
  auto meta = CreateServiceAccountForTest();
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("ServiceAccount={"));
  EXPECT_THAT(actual, HasSubstr("service-123@example.com"));
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
