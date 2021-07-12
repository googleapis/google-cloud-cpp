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
#include "google/cloud/bigquery/bigquery_read_client.h"
#include "google/cloud/bigquery/mocks/mock_bigquery_read_connection.h"
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::google::cloud::bigquery_mocks::MockBigQueryReadConnection;
namespace bigquery = ::google::cloud::bigquery;
//! [helper-aliases]

TEST(MockCreateReadSessionExample, CreateReadSession) {
  //! [create-mock]
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  //! [create-mock]

  //! [setup-expectations]
  EXPECT_CALL(*mock, CreateReadSession)
      .WillOnce([&](google::cloud::bigquery::storage::v1::
                        CreateReadSessionRequest const& request) {
        EXPECT_EQ("test-project-name", request.parent());
        EXPECT_EQ("test-table-name", request.read_session().table());
        google::cloud::bigquery::storage::v1::ReadSession response;
        google::cloud::bigquery::storage::v1::ReadStream stream;
        stream.set_name("test-stream");
        *response.add_streams() = stream;
        return google::cloud::StatusOr<
            google::cloud::bigquery::storage::v1::ReadSession>(response);
      });
  //! [setup-expectations]

  //! [create-client]
  bigquery::BigQueryReadClient bigquery_client(mock);
  //! [create-client]

  //! [client-call]
  google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_table("test-table-name");
  int max_streams = 1;
  auto session = bigquery_client.CreateReadSession("test-project-name",
                                                   read_session, max_streams);
  //! [client-call]

  //! [expected-results]
  EXPECT_TRUE(session.ok());
  EXPECT_EQ("test-stream", session->streams(0).name());
  //! [expected-results]
}

}  // namespace

//! [all]
