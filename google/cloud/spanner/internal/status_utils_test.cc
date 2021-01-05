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

#include "google/cloud/spanner/internal/status_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(StatusUtils, SessionNotFound) {
  google::cloud::Status const session_not_found(StatusCode::kNotFound,
                                                "Session not found");
  EXPECT_TRUE(spanner_internal::IsSessionNotFound(session_not_found));

  google::cloud::Status const other_not_found(StatusCode::kNotFound,
                                              "Other not found");
  EXPECT_FALSE(spanner_internal::IsSessionNotFound(other_not_found));

  google::cloud::Status const not_not_found(StatusCode::kUnavailable,
                                            "Session not found");
  EXPECT_FALSE(spanner_internal::IsSessionNotFound(not_not_found));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
