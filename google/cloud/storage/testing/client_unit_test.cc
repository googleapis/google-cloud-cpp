// Copyright 2021 Google LLC
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

#include "google/cloud/storage/testing/client_unit_test.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

using ::testing::Return;
using ::testing::ReturnRef;

ClientUnitTest::ClientUnitTest()
    : mock_(std::make_shared<testing::MockClient>()),
      client_options_(ClientOptions(oauth2::CreateAnonymousCredentials())) {
  EXPECT_CALL(*mock_, client_options())
      .WillRepeatedly(ReturnRef(client_options_));
  EXPECT_CALL(*mock_, options)
      .WillRepeatedly(Return(storage::internal::DefaultOptionsWithCredentials(
          Options{}
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
              .set<AuthorityOption>("a-default")
              .set<UserProjectOption>("u-p-default"))));
}

Client ClientUnitTest::ClientForMock() {
  return ::google::cloud::storage::testing::ClientFromMock(
      mock_, LimitedErrorCountRetryPolicy(2),
      ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                               std::chrono::milliseconds(1), 2.0));
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
