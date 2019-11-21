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
#include "google/cloud/spanner/internal/time.h"
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

using ::testing::UnorderedElementsAreArray;

// A helper function used in the test fixtures below. This function writes the
// given data to the DataTypes table, then it reads all the data back and
// returns it to the caller.
template <typename T>
StatusOr<T> WriteReadData(Client& client, T const& data,
                          std::string const& column) {
  auto commit_result = client.Commit(
      [&data, &column](Transaction const&) -> StatusOr<Mutations> {
        Mutations mutations;
        int id = 0;
        for (auto const& x : data) {
          mutations.push_back(MakeInsertMutation(
              "DataTypes", {"Id", column}, "Id-" + std::to_string(id++), x));
        }
        return mutations;
      });
  if (!commit_result) return commit_result.status();

  T actual;
  auto rows = client.Read("DataTypes", KeySet::All(), {column});
  using RowType = std::tuple<typename T::value_type>;
  for (auto const& row : StreamOf<RowType>(rows)) {
    if (!row) return row.status();
    actual.push_back(std::get<0>(*row));
  }
  return actual;
}

class DataTypeIntegrationTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    client_ = google::cloud::internal::make_unique<Client>(
        MakeConnection(spanner_testing::DatabaseEnvironment::GetDatabase()));
  }

  void SetUp() override {
    auto commit_result = client_->Commit([](Transaction const&) {
      return Mutations{
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

TEST_F(DataTypeIntegrationTest, WriteReadBool) {
  std::vector<bool> const data = {true, false};
  auto result = WriteReadData(*client_, data, "BoolValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadInt64) {
  std::vector<std::int64_t> const data = {
      std::numeric_limits<std::int64_t>::min(), -123, -42, -1, 0, 1, 42, 123,
      std::numeric_limits<std::int64_t>::max(),
  };
  auto result = WriteReadData(*client_, data, "Int64Value");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadFloat64) {
  std::vector<double> const data = {
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::lowest(),
      std::numeric_limits<double>::min(),
      -123.456,
      -123,
      -42.42,
      -42,
      -1.5,
      -1,
      -0.5,
      0,
      0.5,
      1,
      1.5,
      42,
      42.42,
      123,
      123.456,
      std::numeric_limits<double>::max(),
      std::numeric_limits<double>::infinity(),
  };
  auto result = WriteReadData(*client_, data, "Float64Value");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadFloat64NaN) {
  // Since NaN is not equal to anything, including itself, we need to handle
  // NaN separately from other Float64 values.
  std::vector<double> const data = {
      std::numeric_limits<double>::quiet_NaN(),
  };
  auto result = WriteReadData(*client_, data, "Float64Value");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(1, result->size());
  EXPECT_TRUE(std::isnan(result->front()));
}

TEST_F(DataTypeIntegrationTest, WriteReadString) {
  std::vector<std::string> const data = {
      "",
      "a",
      "Hello World",
      "123456789012345678901234567890",
      std::string(1024, 'x'),
  };
  auto result = WriteReadData(*client_, data, "StringValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadBytes) {
  // Makes a blob containing unprintable characters.
  std::string blob;
  for (char c = std::numeric_limits<char>::min();
       c != std::numeric_limits<char>::max(); ++c) {
    blob.push_back(c);
  }
  std::vector<Bytes> const data = {
      Bytes(""),
      Bytes("a"),
      Bytes("Hello World"),
      Bytes("123456789012345678901234567890"),
      Bytes(blob),
  };
  auto result = WriteReadData(*client_, data, "BytesValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadTimestamp) {
  // TODO(#1098): `Timestamp` cannot represent these extreme values.
  // auto min = internal::TimestampFromString("0001-01-01T00:00:00Z");
  // auto max = internal::TimestampFromString("9999-12-31T23:59:59.999999999Z");

  // ASSERT_STATUS_OK(min);
  // ASSERT_STATUS_OK(max);

  std::vector<Timestamp> const data = {
      // *min,  // TODO(#1098)
      Timestamp(std::chrono::seconds(-1)),
      Timestamp(std::chrono::nanoseconds(-1)),
      Timestamp(std::chrono::seconds(0)),
      Timestamp(std::chrono::nanoseconds(1)),
      Timestamp(std::chrono::seconds(1)),
      Timestamp(std::chrono::system_clock::now()),
      // *max,  // TODO(#1098)
  };
  auto result = WriteReadData(*client_, data, "TimestampValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadDate) {
  std::vector<Date> const data = {
      Date(1, 1, 1),       //
      Date(161, 3, 8),     //
      Date(),              //
      Date(2019, 11, 21),  //
      Date(9999, 12, 31),  //
  };
  auto result = WriteReadData(*client_, data, "DateValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
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
