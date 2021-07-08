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
#include "google/cloud/iam/iam_credentials_client.h"
#include "google/cloud/iam/mocks/mock_iam_credentials_connection.h"
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::google::cloud::iam_mocks::MockIAMCredentialsConnection;
namespace iam = ::google::cloud::iam;
//! [helper-aliases]

TEST(MockSignJwtExample, SignJwt) {
  //! [create-mock]
  auto mock = std::make_shared<MockIAMCredentialsConnection>();
  //! [create-mock]

  //! [setup-expectations]
  EXPECT_CALL(*mock, SignJwt)
      .WillOnce(
          [&](google::iam::credentials::v1::SignJwtRequest const& request) {
            EXPECT_EQ("projects/-/serviceAccounts/test-account-unique-id",
                      request.name());
            google::iam::credentials::v1::SignJwtResponse response;
            response.set_key_id("test-key-id");
            return google::cloud::StatusOr<
                google::iam::credentials::v1::SignJwtResponse>(response);
          });
  //! [setup-expectations]

  //! [create-client]
  iam::IAMCredentialsClient iam_credentials_client(mock);
  //! [create-client]

  //! [client-call]
  std::string payload;
  auto response = iam_credentials_client.SignJwt(
      "projects/-/serviceAccounts/test-account-unique-id", {}, payload);
  //! [client-call]

  //! [expected-results]
  EXPECT_TRUE(response.ok());
  EXPECT_EQ("test-key-id", response->key_id());
  //! [expected-results]
}

}  // namespace

//! [all]
