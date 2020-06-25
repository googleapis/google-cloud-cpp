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

#include "google/cloud/bigquery/internal/connection_impl.h"
#include "google/cloud/bigquery/internal/storage_stub.h"
#include "google/cloud/bigquery/testing/mock_storage_stub.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <memory>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {
namespace {

namespace bigquerystorage_proto = ::google::cloud::bigquery::storage::v1beta1;

using ::google::cloud::Status;
using ::google::cloud::StatusCode;
using ::google::cloud::StatusOr;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;

TEST(ConnectionImplTest, ParallelReadTableFailure) {
  auto mock = std::make_shared<bigquery_testing::MockStorageStub>();
  auto conn = MakeConnection(mock);

  {
    StatusOr<std::vector<ReadStream>> result = conn->ParallelRead(
        "my-parent-project", "my-project.my-dataset.my-table", {});
    EXPECT_THAT(result.status().code(), Eq(StatusCode::kInvalidArgument));
    EXPECT_THAT(
        result.status().message(),
        Eq("Table name must be of the form PROJECT_ID:DATASET_ID.TABLE_ID."));
  }

  {
    StatusOr<std::vector<ReadStream>> result = conn->ParallelRead(
        "my-parent-project", "my-project:my-dataset:my-table", {});
    EXPECT_THAT(result.status().code(), Eq(StatusCode::kInvalidArgument));
    EXPECT_THAT(
        result.status().message(),
        Eq("Table name must be of the form PROJECT_ID:DATASET_ID.TABLE_ID."));
  }
}

TEST(ConnectionImplTest, ParallelReadRpcFailure) {
  auto mock = std::make_shared<bigquery_testing::MockStorageStub>();
  auto conn = MakeConnection(mock);
  EXPECT_CALL(*mock, CreateReadSession(_))
      .WillOnce([](bigquerystorage_proto::
                       CreateReadSessionRequest const& /*request*/) {
        return Status(StatusCode::kPermissionDenied, "Permission denied!");
      });

  StatusOr<std::vector<ReadStream>> result = conn->ParallelRead(
      "my-parent-project", "my-project:my-dataset.my-table", {});
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kPermissionDenied));
  EXPECT_THAT(result.status().message(), Eq("Permission denied!"));
}

TEST(ConnectionImplTest, ParallelReadRpcSuccess) {
  auto mock = std::make_shared<bigquery_testing::MockStorageStub>();
  auto conn = MakeConnection(mock);
  EXPECT_CALL(*mock, CreateReadSession(_))
      .WillOnce(
          [](bigquerystorage_proto::CreateReadSessionRequest const& request)
              -> StatusOr<bigquerystorage_proto::ReadSession> {
            EXPECT_THAT(request.parent(), Eq("projects/my-parent-project"));
            EXPECT_THAT(request.table_reference().project_id(),
                        Eq("my-project"));
            EXPECT_THAT(request.table_reference().dataset_id(),
                        Eq("my-dataset"));
            EXPECT_THAT(request.table_reference().table_id(), Eq("my-table"));

            EXPECT_THAT(request.read_options().selected_fields_size(), Eq(2));
            EXPECT_THAT(request.read_options().selected_fields(0), Eq("col-0"));
            EXPECT_THAT(request.read_options().selected_fields(1), Eq("col-1"));

            bigquerystorage_proto::ReadSession response;
            std::string const text = R"pb(
              name: "my-session"
              streams { name: "stream-0" }
              streams { name: "stream-1" }
              streams { name: "stream-2" }
            )pb";
            EXPECT_TRUE(TextFormat::ParseFromString(text, &response));
            return response;
          });

  StatusOr<std::vector<ReadStream>> result =
      conn->ParallelRead("my-parent-project", "my-project:my-dataset.my-table",
                         {"col-0", "col-1"});
  EXPECT_THAT(result.ok(), IsTrue());
  EXPECT_THAT(result.value(), ElementsAre(MakeReadStream("stream-0"),
                                          MakeReadStream("stream-1"),
                                          MakeReadStream("stream-2")));
}

}  // namespace
}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
