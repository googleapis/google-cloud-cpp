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
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::UnorderedElementsAreArray;

std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
MakeTimePoint(std::time_t sec, std::chrono::nanoseconds::rep nanos) {
  return std::chrono::system_clock::from_time_t(sec) +
         std::chrono::nanoseconds(nanos);
}

// A helper function used in the test fixtures below. This function writes the
// given data to the DataTypes table, then it reads all the data back and
// returns it to the caller.
template <typename T>
StatusOr<T> WriteReadData(Client& client, T const& data,
                          std::string const& column) {
  Mutations mutations;
  int id = 0;
  for (auto const& x : data) {
    mutations.push_back(MakeInsertMutation("DataTypes", {"Id", column},
                                           "Id-" + std::to_string(id++), x));
  }
  auto commit_result = client.Commit(std::move(mutations));
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

class DataTypeIntegrationTest
    : public spanner_testing::DatabaseIntegrationTest {
 public:
  static void SetUpTestSuite() {
    spanner_testing::DatabaseIntegrationTest::SetUpTestSuite();
    client_ = absl::make_unique<Client>(MakeConnection(GetDatabase()));
  }

  void SetUp() override {
    auto commit_result = client_->Commit(Mutations{
        MakeDeleteMutation("DataTypes", KeySet::All()),
    });
    EXPECT_STATUS_OK(commit_result);
  }

  static void TearDownTestSuite() {
    client_ = nullptr;
    spanner_testing::DatabaseIntegrationTest::TearDownTestSuite();
  }

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
  auto min = internal::TimestampFromRFC3339("0001-01-01T00:00:00Z");
  ASSERT_STATUS_OK(min);
  auto max = internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z");
  ASSERT_STATUS_OK(max);
  auto now = MakeTimestamp(std::chrono::system_clock::now());
  ASSERT_STATUS_OK(now);

  std::vector<Timestamp> const data = {
      *min,
      MakeTimestamp(MakeTimePoint(-1, 0)).value(),
      MakeTimestamp(MakeTimePoint(0, -1)).value(),
      MakeTimestamp(MakeTimePoint(0, 0)).value(),
      MakeTimestamp(MakeTimePoint(0, 1)).value(),
      MakeTimestamp(MakeTimePoint(1, 0)).value(),
      *now,
      *max,
  };
  auto result = WriteReadData(*client_, data, "TimestampValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadDate) {
  std::vector<absl::CivilDay> const data = {
      absl::CivilDay(1, 1, 1),       //
      absl::CivilDay(161, 3, 8),     //
      absl::CivilDay(),              //
      absl::CivilDay(2019, 11, 21),  //
      absl::CivilDay(9999, 12, 31),  //
  };
  auto result = WriteReadData(*client_, data, "DateValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayBool) {
  std::vector<std::vector<bool>> const data = {
      std::vector<bool>{},
      std::vector<bool>{true},
      std::vector<bool>{false},
      std::vector<bool>{true, false},
      std::vector<bool>{false, true},
  };
  auto result = WriteReadData(*client_, data, "ArrayBoolValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayInt64) {
  std::vector<std::vector<std::int64_t>> const data = {
      std::vector<std::int64_t>{},
      std::vector<std::int64_t>{-1},
      std::vector<std::int64_t>{-1, 0, 1},
  };
  auto result = WriteReadData(*client_, data, "ArrayInt64Value");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayFloat64) {
  std::vector<std::vector<double>> const data = {
      std::vector<double>{},
      std::vector<double>{-0.5},
      std::vector<double>{-0.5, 0.5, 1.5},
  };
  auto result = WriteReadData(*client_, data, "ArrayFloat64Value");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayString) {
  std::vector<std::vector<std::string>> const data = {
      std::vector<std::string>{},
      std::vector<std::string>{""},
      std::vector<std::string>{"", "foo", "bar"},
  };
  auto result = WriteReadData(*client_, data, "ArrayStringValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayBytes) {
  std::vector<std::vector<Bytes>> const data = {
      std::vector<Bytes>{},
      std::vector<Bytes>{Bytes("")},
      std::vector<Bytes>{Bytes(""), Bytes("foo"), Bytes("bar")},
  };
  auto result = WriteReadData(*client_, data, "ArrayBytesValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayTimestamp) {
  std::vector<std::vector<Timestamp>> const data = {
      std::vector<Timestamp>{},
      std::vector<Timestamp>{MakeTimestamp(MakeTimePoint(-1, 0)).value()},
      std::vector<Timestamp>{
          MakeTimestamp(MakeTimePoint(-1, 0)).value(),
          MakeTimestamp(MakeTimePoint(0, 0)).value(),
          MakeTimestamp(MakeTimePoint(1, 0)).value(),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayTimestampValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayDate) {
  std::vector<std::vector<absl::CivilDay>> const data = {
      std::vector<absl::CivilDay>{},
      std::vector<absl::CivilDay>{absl::CivilDay()},
      std::vector<absl::CivilDay>{
          absl::CivilDay(1, 1, 1),
          absl::CivilDay(),
          absl::CivilDay(9999, 12, 31),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayDateValue");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, UnorderedElementsAreArray(data));
}

// This test differs a lot from the other tests since Spanner STRUCT types may
// not be used as column types, and they may not be returned as top-level
// objects in a select statement. See
// https://cloud.google.com/spanner/docs/data-types#struct-type for more info.
//
// So the approach taken here instead is to use a STRUCT (i.e., a std::tuple<>
// in the C++ world), as a bound SQL query parameter to insert some data into
// the table. This, in a way, tests the "write" path.
//
// To test the "read" path, we create a query that returns an array of struct,
// that we then compare to the original data.
TEST_F(DataTypeIntegrationTest, InsertAndQueryWithStruct) {
  using StructType =
      std::tuple<std::pair<std::string, std::string>,
                 std::pair<std::string, std::vector<std::int64_t>>>;
  auto data = StructType{{"StringValue", "xx"}, {"ArrayInt64Value", {1, 2, 3}}};

  auto& client = *client_;
  auto commit_result = client.Commit(
      [&data, &client](Transaction const& txn) -> StatusOr<Mutations> {
        auto dml_result = client.ExecuteDml(
            txn,
            SqlStatement(
                "INSERT INTO DataTypes (Id, StringValue, ArrayInt64Value)"
                "VALUES(@id, @struct.StringValue, @struct.ArrayInt64Value)",
                {{"id", Value("id-1")}, {"struct", Value(data)}}));
        if (!dml_result) return dml_result.status();
        return Mutations{};
      });
  ASSERT_STATUS_OK(commit_result);

  auto rows = client_->ExecuteQuery(
      SqlStatement("SELECT ARRAY(SELECT STRUCT(StringValue, ArrayInt64Value)) "
                   "FROM DataTypes"));
  using RowType = std::tuple<std::vector<StructType>>;
  auto row = GetSingularRow(StreamOf<RowType>(rows));
  ASSERT_STATUS_OK(row);

  auto const& v = std::get<0>(*row);
  EXPECT_EQ(1, v.size());
  EXPECT_EQ(data, v[0]);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
