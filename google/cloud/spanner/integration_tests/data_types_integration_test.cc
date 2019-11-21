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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/database_environment.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

class DataTypeIntegrationTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    client_ = google::cloud::internal::make_unique<Client>(
        MakeConnection(spanner_testing::DatabaseEnvironment::GetDatabase()));
  }

  void SetUp() override {
    auto commit_result = client_->Commit([](Transaction const&) {
      return Mutations{
          MakeDeleteMutation("Singers", KeySet::All()),
          MakeDeleteMutation("DataTypes", KeySet::All()),
      };
    });
    EXPECT_STATUS_OK(commit_result);
  }

  static void TearDownTestSuite() { client_ = nullptr; }

 protected:
  static std::unique_ptr<Client> client_;
};

std::unique_ptr<Client> DataTypeIntegrationTest::client_;

TEST_F(DataTypeIntegrationTest, ReadWriteDate) {
  auto& client = *client_;
  using RowType = std::tuple<std::string, Date, std::string>;
  RowType read_back;
  auto commit_result = client_->Commit(
      [&client, &read_back](Transaction const& txn) -> StatusOr<Mutations> {
        auto result = client.ExecuteDml(
            txn,
            SqlStatement(
                "INSERT INTO DataTypes (Id, DateValue, StringValue) "
                "VALUES(@id, @date, @event)",
                {{"id", Value("ReadWriteDate-1")},
                 {"date", Value(Date(161, 3, 8))},
                 {"event", Value("Marcus Aurelius ascends to the throne")}}));
        if (!result) return std::move(result).status();

        auto reader = client.ExecuteQuery(
            txn, SqlStatement("SELECT Id, DateValue, StringValue FROM DataTypes"
                              " WHERE Id = @id",
                              {{"id", Value("ReadWriteDate-1")}}));

        auto row = GetSingularRow(StreamOf<RowType>(reader));
        if (!row) return std::move(row).status();
        read_back = *std::move(row);
        return Mutations{};
      });

  ASSERT_TRUE(commit_result.ok());
  EXPECT_EQ("ReadWriteDate-1", std::get<0>(read_back));
  EXPECT_EQ(Date(161, 3, 8), std::get<1>(read_back));
  EXPECT_EQ("Marcus Aurelius ascends to the throne", std::get<2>(read_back));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::google::cloud::testing_util::InitGoogleMock(argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::spanner_testing::DatabaseEnvironment());

  return RUN_ALL_TESTS();
}
