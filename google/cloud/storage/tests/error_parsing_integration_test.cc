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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

using ErrorParsingIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ErrorParsingIntegrationTest, FailureContainsErrorInfo) {
  // TODO(#9947) - enable with new implementation once fixed.
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP", "any");
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Start a resumable upload and finalize the upload.
  auto insert = client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                                     IfGenerationMatch(0));
  ASSERT_THAT(insert, StatusIs(StatusCode::kOk));
  ScheduleForDelete(*insert);

  // Overwrite the object.
  insert = client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                                IfGenerationMatch(0));
  ASSERT_THAT(insert, Not(StatusIs(StatusCode::kOk)));
  if (UsingEmulator()) return;
  EXPECT_THAT(insert.status().message(),
              HasSubstr("pre-conditions you specified did not hold"));
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
