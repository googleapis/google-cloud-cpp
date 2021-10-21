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

#include "google/cloud/storage/internal/service_account_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(ServiceAccountRequests, Get) {
  GetProjectServiceAccountRequest request("project-to-get");
  EXPECT_EQ("project-to-get", request.project_id());

  request.set_multiple_options(UserProject("project-to-bill"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("GetProjectServiceAccountRequest"));
  EXPECT_THAT(os.str(), HasSubstr("project_id=project-to-get"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=project-to-bill"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
