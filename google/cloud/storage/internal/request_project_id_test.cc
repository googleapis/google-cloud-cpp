// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/request_project_id.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/override_default_project.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;

google::cloud::internal::ErrorInfoBuilder TestErrorInfo() {
  return GCP_ERROR_INFO().WithMetadata("test-key", "test-value");
}

TEST(RequestProjectId, NotSet) {
  auto const actual = RequestProjectId(TestErrorInfo(), Options{},
                                       storage::UserProject("unused-1"));
  ASSERT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("missing project id")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              Contains(Pair("test-key", "test-value")));
}

TEST(RequestProjectId, FromOptions) {
  auto const actual = RequestProjectId(
      TestErrorInfo(),
      Options{}.set<storage::ProjectIdOption>("options-project-id"),
      storage::UserProject("unused-1"));
  EXPECT_THAT(actual, IsOkAndHolds("options-project-id"));
}

TEST(RequestProjectId, FromOverride) {
  auto const actual = RequestProjectId(
      TestErrorInfo(),
      Options{}.set<storage::ProjectIdOption>("options-project-id"),
      storage::UserProject("unused-1"),
      storage::OverrideDefaultProject("override-project-id"),
      storage::UserProject("unused-2"));
  EXPECT_THAT(actual, IsOkAndHolds("override-project-id"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
