// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/testing/metadata_test_utils.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/rest_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::ElementsAre;
using ::testing::IsEmpty;

static auto const kUserProject = "test-only-project";
static auto const kQuotaUser = "test-quota-user";

void VerifyMetadataContext(rest_internal::RestContext& context) {
  EXPECT_THAT(context.GetHeader("x-goog-api-client"),
              ElementsAre(internal::HandCraftedLibClientHeader()));
  EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
  EXPECT_THAT(context.GetHeader("x-goog-user-project"),
              ElementsAre(kUserProject));
  EXPECT_THAT(context.GetHeader("x-goog-quota-user"), ElementsAre(kQuotaUser));
  EXPECT_THAT(context.GetHeader("x-server-timeout"), ElementsAre("3.141"));
}

Options GetMetadataOptions() {
  return Options{}
      .set<UserProjectOption>(kUserProject)
      .set<QuotaUserOption>(kQuotaUser)
      .set<ServerTimeoutOption>(std::chrono::milliseconds(3141));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
