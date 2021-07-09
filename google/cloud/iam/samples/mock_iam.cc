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

//! [all]
//! [required-includes]
#include "google/cloud/iam/iam_client.h"
#include "google/cloud/iam/mocks/mock_iam_connection.h"
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::google::cloud::iam_mocks::MockIAMConnection;
namespace iam = ::google::cloud::iam;
//! [helper-aliases]

TEST(MockGetServiceAccountExample, GetServiceAccount) {
  //! [create-mock]
  auto mock = std::make_shared<MockIAMConnection>();
  //! [create-mock]

  //! [setup-expectations]
  EXPECT_CALL(*mock, GetServiceAccount)
      .WillOnce([&](google::iam::admin::v1::GetServiceAccountRequest const&
                        request) {
        EXPECT_EQ("test-project-name", request.name());
        google::iam::admin::v1::ServiceAccount service_account;
        service_account.set_unique_id("test-unique-id");
        return google::cloud::StatusOr<google::iam::admin::v1::ServiceAccount>(
            service_account);
      });
  //! [setup-expectations]

  //! [create-client]
  iam::IAMClient iam_client(mock);
  //! [create-client]

  //! [client-call]
  auto service_account = iam_client.GetServiceAccount("test-project-name");
  //! [client-call]

  //! [expected-results]
  EXPECT_TRUE(service_account.ok());
  EXPECT_EQ("test-unique-id", service_account->unique_id());
  //! [expected-results]
}

}  // namespace

//! [all]
