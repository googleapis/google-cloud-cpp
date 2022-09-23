// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/access_control_common_parser.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
// This file contains tests for deprecated functions, we need to disable the
// warnings
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;

TEST(AccessControlCommonParserTest, FromJson) {
  std::string bucket = "bucket-0";
  std::string role = internal::AccessControlCommon::ROLE_OWNER();
  std::string email = "foo@example.com";
  nlohmann::json metadata{{"bucket", bucket}, {"role", role}, {"email", email}};
  internal::AccessControlCommon result{};
  auto status = internal::AccessControlCommonParser::FromJson(result, metadata);
  ASSERT_STATUS_OK(status);

  EXPECT_THAT(result.bucket(), Eq(bucket));
  EXPECT_THAT(result.role(), Eq(role));
  EXPECT_THAT(result.email(), Eq(email));
  EXPECT_FALSE(result.has_project_team());
}

TEST(AccessControlCommonParserTest, NullProjectTeamIsValid) {
  nlohmann::json metadata{{"projectTeam", nullptr}};
  internal::AccessControlCommon result{};
  auto status = internal::AccessControlCommonParser::FromJson(result, metadata);
  ASSERT_STATUS_OK(status);

  EXPECT_FALSE(result.has_project_team());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
