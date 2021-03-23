// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/make_jwt_assertion.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;

TEST(MakeJWTAssertionNoThrow, Basic) {
  auto header = nlohmann::json{
      {"alg", "HS256"}, {"typ", "JWT"}, {"kid", "test-key-name"}};
  auto payload = nlohmann::json{
      {"iss", "--invalid--@developer.gserviceaccount.com"},
      {"sub", "--invalid--@developer.gserviceaccount.com"},
      {"aud", "https//not-a-service.googleapis.com"},
      {"iat", "1511900000"},
      {"exp", "1511903600"},
  };
  auto const assertion = MakeJWTAssertionNoThrow(header.dump(), payload.dump(),
                                                 testing::kWellFormatedKey);
  ASSERT_THAT(assertion, IsOk());

  std::vector<std::string> components = absl::StrSplit(*assertion, '.');
  EXPECT_EQ(components.size(), 3);
}

TEST(MakeJWTAssertionNoThrow, InvalidKey) {
  auto header = nlohmann::json{
      {"alg", "HS256"}, {"typ", "JWT"}, {"kid", "test-key-name"}};
  auto payload = nlohmann::json{
      {"iss", "--invalid--@developer.gserviceaccount.com"},
      {"sub", "--invalid--@developer.gserviceaccount.com"},
      {"aud", "https//not-a-service.googleapis.com"},
      {"iat", "1511900000"},
      {"exp", "1511903600"},
  };
  auto const assertion =
      MakeJWTAssertionNoThrow(header.dump(), payload.dump(), "invalid-key");
  ASSERT_THAT(assertion, Not(IsOk()));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
