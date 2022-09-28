// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/testing/status_utils.h"
#include "google/cloud/internal/status_payload_keys.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(StatusUtils, SessionNotFound) {
  Status const session_not_found(StatusCode::kNotFound, "Session not found");
  EXPECT_TRUE(IsSessionNotFound(session_not_found)) << session_not_found;

  Status const other_not_found(StatusCode::kNotFound, "Other not found");
  EXPECT_FALSE(IsSessionNotFound(other_not_found)) << other_not_found;

  Status const not_not_found(StatusCode::kUnavailable, "Session not found");
  EXPECT_FALSE(IsSessionNotFound(not_not_found)) << not_not_found;
}

TEST(StatusUtils, SessionNotFoundResourceInfo) {
  auto db = spanner::Database("project", "instance", "database");
  auto name = db.FullName() + "/sessions/session";
  auto not_found = spanner_testing::SessionNotFoundError(std::move(name));
  EXPECT_TRUE(IsSessionNotFound(not_found)) << not_found;

  // A status with the right ResourceInfo doesn't need a particular message.
  Status still_not_found(not_found.code(), "foo bar", not_found.error_info());
  std::string const key = internal::kStatusPayloadGrpcProto;
  auto payload = internal::GetPayload(not_found, key);
  ASSERT_TRUE(payload.has_value());
  internal::SetPayload(still_not_found, key, *std::move(payload));
  EXPECT_NE(not_found, still_not_found);
  EXPECT_TRUE(IsSessionNotFound(still_not_found)) << still_not_found;
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
