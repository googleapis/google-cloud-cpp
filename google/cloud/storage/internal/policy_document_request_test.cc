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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(PolicyDocumentRequest, SigningAccount) {
  PolicyDocumentRequest request;
  EXPECT_FALSE(request.signing_account().has_value());
  EXPECT_FALSE(request.signing_account_delegates().has_value());

  request.set_multiple_options(
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));
  ASSERT_TRUE(request.signing_account().has_value());
  ASSERT_TRUE(request.signing_account_delegates().has_value());
  EXPECT_EQ("another-account@example.com", request.signing_account().value());
  EXPECT_THAT(request.signing_account_delegates().value(),
              ::testing::ElementsAre("test-delegate1", "test-delegate2"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
