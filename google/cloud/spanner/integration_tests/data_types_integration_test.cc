// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/time.h"
#include <google/cloud/spanner/testing/singer.pb.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAreArray;

absl::Time MakeTime(std::time_t sec, int nanos) {
  return absl::FromTimeT(sec) + absl::Nanoseconds(nanos);
}

testing::SingerInfo MakeSinger(std::int64_t singer_id, std::string birth_date,
                               std::string nationality, testing::Genre genre) {
  testing::SingerInfo singer;
  singer.set_singer_id(singer_id);
  singer.set_birth_date(std::move(birth_date));
  singer.set_nationality(std::move(nationality));
  singer.set_genre(genre);
  return singer;
}

// A helper function used in the test fixtures below. This function writes the
// given data to the DataTypes table, then it reads all the data back and
// returns it to the caller.
template <typename T>
StatusOr<T> WriteReadData(Client& client, T const& data,
                          std::string const& column) {
  Mutations mutations;
  int id = 0;
  for (auto&& x : data) {
    mutations.push_back(MakeInsertMutation("DataTypes", {"Id", column},
                                           "Id-" + std::to_string(++id), x));
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

template <typename T>
class DataTypeIntegrationTestTmpl : public T {
 protected:
  static void SetUpTestSuite() {
    T::SetUpTestSuite();
    client_ = std::make_unique<Client>(MakeConnection(T::GetDatabase()));
  }

  void SetUp() override {
    auto commit_result = client_->Commit(Mutations{
        MakeDeleteMutation("DataTypes", KeySet::All()),
    });
    ASSERT_THAT(commit_result, IsOk());
  }

  static void TearDownTestSuite() {
    client_ = nullptr;
    T::TearDownTestSuite();
  }

  static std::unique_ptr<Client> client_;
};

template <typename T>
std::unique_ptr<Client> DataTypeIntegrationTestTmpl<T>::client_;

using DataTypeIntegrationTest =
    DataTypeIntegrationTestTmpl<spanner_testing::DatabaseIntegrationTest>;
using PgDataTypeIntegrationTest =
    DataTypeIntegrationTestTmpl<spanner_testing::PgDatabaseIntegrationTest>;

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
  EXPECT_THAT(result, IsOkAndHolds(ElementsAre(ResultOf(
                          [](double d) { return std::isnan(d); }, true))));
}

TEST_F(DataTypeIntegrationTest, WriteReadFloat32) {
  std::vector<float> const data = {
      -std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::min(),
      -123.456F,
      -123,
      -42.42F,
      -42,
      -1.5,
      -1,
      -0.5,
      0,
      0.5,
      1,
      1.5,
      42,
      42.42F,
      123,
      123.456F,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::infinity(),
  };
  auto result = WriteReadData(*client_, data, "Float32Value");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    ASSERT_STATUS_OK(result);
    EXPECT_THAT(*result, UnorderedElementsAreArray(data));
  }
}

TEST_F(PgDataTypeIntegrationTest, WriteReadFloat32) {
  std::vector<float> const data = {
      -std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::min(),
      -123.456F,
      -123,
      -42.42F,
      -42,
      -1.5,
      -1,
      -0.5,
      0,
      0.5,
      1,
      1.5,
      42,
      42.42F,
      123,
      123.456F,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::infinity(),
  };
  auto result = WriteReadData(*client_, data, "Float32Value");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    ASSERT_STATUS_OK(result);
    EXPECT_THAT(*result, UnorderedElementsAreArray(data));
  }
}

TEST_F(DataTypeIntegrationTest, WriteReadFloat32NaN) {
  // Since NaN is not equal to anything, including itself, we need to handle
  // NaN separately from other Float32 values.
  std::vector<float> const data = {
      std::numeric_limits<float>::quiet_NaN(),
  };
  auto result = WriteReadData(*client_, data, "Float32Value");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(ElementsAre(ResultOf(
                            [](float d) { return std::isnan(d); }, true))));
  }
}

TEST_F(PgDataTypeIntegrationTest, WriteReadFloat32NaN) {
  // Since NaN is not equal to anything, including itself, we need to handle
  // NaN separately from other Float32 values.
  std::vector<float> const data = {
      std::numeric_limits<float>::quiet_NaN(),
  };
  auto result = WriteReadData(*client_, data, "Float32Value");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(ElementsAre(ResultOf(
                            [](float d) { return std::isnan(d); }, true))));
  }
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
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
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
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadTimestamp) {
  auto min = spanner_internal::TimestampFromRFC3339("0001-01-01T00:00:00Z");
  ASSERT_STATUS_OK(min);
  auto max =
      spanner_internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z");
  ASSERT_STATUS_OK(max);
  auto now = MakeTimestamp(std::chrono::system_clock::now());
  ASSERT_STATUS_OK(now);

  std::vector<Timestamp> const data = {
      *min,
      MakeTimestamp(MakeTime(-1, 0)).value(),
      MakeTimestamp(MakeTime(0, -1)).value(),
      MakeTimestamp(MakeTime(0, 0)).value(),
      MakeTimestamp(MakeTime(0, 1)).value(),
      MakeTimestamp(MakeTime(1, 0)).value(),
      *now,
      *max,
  };
  auto result = WriteReadData(*client_, data, "TimestampValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
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
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadJson) {
  std::vector<Json> const data = {
      Json(),                     //
      Json(R"("Hello world!")"),  //
      Json("42"),                 //
      Json("true"),               //
  };
  auto result = WriteReadData(*client_, data, "JsonValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(PgDataTypeIntegrationTest, WriteReadJson) {
  std::vector<JsonB> const data = {
      JsonB(),                     //
      JsonB(R"("Hello world!")"),  //
      JsonB("42"),                 //
      JsonB("true"),               //
  };
  auto result = WriteReadData(*client_, data, "JsonValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadNumeric) {
  auto min = MakeNumeric("-99999999999999999999999999999.999999999");
  ASSERT_STATUS_OK(min);
  auto max = MakeNumeric("99999999999999999999999999999.999999999");
  ASSERT_STATUS_OK(max);

  std::vector<Numeric> const data = {
      *min,                                //
      MakeNumeric(-999999999e-3).value(),  //
      MakeNumeric(-1).value(),             //
      MakeNumeric(0).value(),              //
      MakeNumeric(1).value(),              //
      MakeNumeric(999999999e-3).value(),   //
      *max,                                //
  };
  auto result = WriteReadData(*client_, data, "NumericValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(PgDataTypeIntegrationTest, WriteReadNumeric) {
  auto limit = std::string(131072, '9') + "." + std::string(16383, '9');
  auto min = MakePgNumeric("-" + limit);
  ASSERT_STATUS_OK(min);
  auto max = MakePgNumeric(limit);
  ASSERT_STATUS_OK(max);

  std::vector<PgNumeric> const data = {
      *min,                                  //
      MakePgNumeric(-999999999e-3).value(),  //
      MakePgNumeric(-1).value(),             //
      MakePgNumeric(0).value(),              //
      MakePgNumeric(1).value(),              //
      MakePgNumeric(999999999e-3).value(),   //
      *max,                                  //
  };
  auto result = WriteReadData(*client_, data, "NumericValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadProtoEnum) {
  std::vector<ProtoEnum<testing::Genre>> const data = {
      testing::Genre::POP,
      testing::Genre::JAZZ,
      testing::Genre::FOLK,
      testing::Genre::ROCK,
  };
  auto result = WriteReadData(*client_, data, "SingerGenre");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
  }
}

TEST_F(DataTypeIntegrationTest, WriteReadProtoMessage) {
  std::vector<ProtoMessage<testing::SingerInfo>> const data = {
      MakeSinger(1, "1817-05-25", "French", testing::Genre::FOLK),
      MakeSinger(2123139547, "1942-06-18", "British", testing::Genre::POP),
  };
  auto result = WriteReadData(*client_, data, "SingerInfo");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
  }
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
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayInt64) {
  std::vector<std::vector<std::int64_t>> const data = {
      std::vector<std::int64_t>{},
      std::vector<std::int64_t>{-1},
      std::vector<std::int64_t>{-1, 0, 1},
  };
  auto result = WriteReadData(*client_, data, "ArrayInt64Value");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayFloat64) {
  std::vector<std::vector<double>> const data = {
      std::vector<double>{},
      std::vector<double>{-0.5},
      std::vector<double>{-0.5, 0.5, 1.5},
  };
  auto result = WriteReadData(*client_, data, "ArrayFloat64Value");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayFloat32) {
  std::vector<std::vector<float>> const data = {
      std::vector<float>{},
      std::vector<float>{-0.5},
      std::vector<float>{-0.5, 0.5, 1.5},
  };
  auto result = WriteReadData(*client_, data, "ArrayFloat32Value");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
  }
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayString) {
  std::vector<std::vector<std::string>> const data = {
      std::vector<std::string>{},
      std::vector<std::string>{""},
      std::vector<std::string>{"", "foo", "bar"},
  };
  auto result = WriteReadData(*client_, data, "ArrayStringValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayBytes) {
  std::vector<std::vector<Bytes>> const data = {
      std::vector<Bytes>{},
      std::vector<Bytes>{Bytes("")},
      std::vector<Bytes>{Bytes(""), Bytes("foo"), Bytes("bar")},
  };
  auto result = WriteReadData(*client_, data, "ArrayBytesValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayTimestamp) {
  std::vector<std::vector<Timestamp>> const data = {
      std::vector<Timestamp>{},
      std::vector<Timestamp>{MakeTimestamp(MakeTime(-1, 0)).value()},
      std::vector<Timestamp>{
          MakeTimestamp(MakeTime(-1, 0)).value(),
          MakeTimestamp(MakeTime(0, 0)).value(),
          MakeTimestamp(MakeTime(1, 0)).value(),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayTimestampValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, SelectIntervalScalar) {
  auto expected_interval = MakeInterval("1-2 3 4:5:6.789123456");
  ASSERT_STATUS_OK(expected_interval);

  auto select = SqlStatement(R"sql(
SELECT INTERVAL '1-2 3 4:5:6.789123456' YEAR TO SECOND;)sql",
                             {});

  auto result = client_->ExecuteQuery(std::move(select));
  for (auto& row : StreamOf<std::tuple<Interval>>(result)) {
    ASSERT_STATUS_OK(row);
    EXPECT_THAT(std::get<0>(row.value()), Eq(*expected_interval));
  }
}

TEST_F(DataTypeIntegrationTest, SelectIntervalArray) {
  auto expected_interval = MakeInterval("1-2 3 4:5:6.789123456");
  ASSERT_STATUS_OK(expected_interval);

  auto select = SqlStatement(R"sql(
SELECT ARRAY<INTERVAL>[INTERVAL '1-2 3 4:5:6.789123456' YEAR TO SECOND];)sql",
                             {});

  auto result = client_->ExecuteQuery(std::move(select));
  for (auto& row : StreamOf<std::tuple<std::vector<Interval>>>(result)) {
    ASSERT_STATUS_OK(row);
    EXPECT_THAT(std::get<0>(row.value()).front(), Eq(*expected_interval));
  }
}

TEST_F(DataTypeIntegrationTest, SelectIntervalFromTimestampDiff) {
  Interval expected_interval{
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::hours(1))};
  auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  auto one_hour_later_seconds =
      now_seconds +
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(1))
          .count();
  std::vector<std::vector<Timestamp>> const data = {std::vector<Timestamp>{
      MakeTimestamp(MakeTime(now_seconds, 0)).value(),
      MakeTimestamp(MakeTime(one_hour_later_seconds, 0)).value()}};
  auto result = WriteReadData(*client_, data, "ArrayTimestampValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));

  auto statement = SqlStatement(R"sql(
SELECT
  CAST(ARRAY_LAST(ArrayTimestampValue) - ARRAY_FIRST(ArrayTimestampValue)
    AS INTERVAL) as v1
FROM DataTypes;
)sql",
                                {});

  auto query_result = client_->ExecuteQuery(std::move(statement));

  using RowType = std::tuple<Interval>;
  for (auto& row : StreamOf<RowType>(query_result)) {
    ASSERT_STATUS_OK(row);
    EXPECT_THAT(std::get<0>(row.value()), Eq(expected_interval));
  }
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
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayJson) {
  std::vector<std::vector<Json>> const data = {
      std::vector<Json>{},
      std::vector<Json>{Json()},
      std::vector<Json>{
          Json(),
          Json(R"("Hello world!")"),
          Json("42"),
          Json("true"),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayJsonValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(PgDataTypeIntegrationTest, WriteReadArrayJson) {
  std::vector<std::vector<JsonB>> const data = {
      std::vector<JsonB>{},
      std::vector<JsonB>{JsonB()},
      std::vector<JsonB>{
          JsonB(),
          JsonB(R"("Hello world!")"),
          JsonB("42"),
          JsonB("true"),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayJsonValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayNumeric) {
  std::vector<std::vector<Numeric>> const data = {
      std::vector<Numeric>{},
      std::vector<Numeric>{Numeric()},
      std::vector<Numeric>{
          MakeNumeric(-1e+9).value(),
          MakeNumeric(1e-9).value(),
          MakeNumeric(1e+9).value(),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayNumericValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(PgDataTypeIntegrationTest, WriteReadArrayNumeric) {
  std::vector<std::vector<PgNumeric>> const data = {
      std::vector<PgNumeric>{},
      std::vector<PgNumeric>{PgNumeric()},
      std::vector<PgNumeric>{
          MakePgNumeric(-1e+9).value(),
          MakePgNumeric(1e-9).value(),
          MakePgNumeric(1e+9).value(),
      },
  };
  auto result = WriteReadData(*client_, data, "ArrayNumericValue");
  EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayProtoEnum) {
  std::vector<std::vector<ProtoEnum<testing::Genre>>> const data = {
      std::vector<ProtoEnum<testing::Genre>>{},
      std::vector<ProtoEnum<testing::Genre>>{
          testing::Genre::POP,
          testing::Genre::JAZZ,
          testing::Genre::FOLK,
          testing::Genre::ROCK,
      },
  };
  auto result = WriteReadData(*client_, data, "ArraySingerGenre");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
  }
}

TEST_F(DataTypeIntegrationTest, WriteReadArrayProtoMessage) {
  std::vector<std::vector<ProtoMessage<testing::SingerInfo>>> const data = {
      std::vector<ProtoMessage<testing::SingerInfo>>{},
      std::vector<ProtoMessage<testing::SingerInfo>>{
          MakeSinger(1, "1817-05-25", "French", testing::Genre::FOLK),
          MakeSinger(2123139547, "1942-06-18", "British", testing::Genre::POP),
      },
  };
  auto result = WriteReadData(*client_, data, "ArraySingerInfo");
  if (UsingEmulator()) {
    EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(UnorderedElementsAreArray(data)));
  }
}

TEST_F(DataTypeIntegrationTest, JsonIndexAndPrimaryKey) {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  // Verify that a JSON column cannot be used as an index.
  std::vector<std::string> statements;
  statements.emplace_back(R"""(
        CREATE INDEX DataTypesByJsonValue
          ON DataTypes(JsonValue)
      )""");
  auto metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  EXPECT_THAT(metadata, StatusIs(StatusCode::kFailedPrecondition,
                                 AnyOf(HasSubstr("unsupported type JSON"),
                                       HasSubstr("Cannot reference JSON"))));

  // Verify that a JSON column cannot be used as a primary key.
  statements.clear();
  statements.emplace_back(R"""(
        CREATE TABLE JsonKey (
          Key JSON NOT NULL
        ) PRIMARY KEY (Key)
      )""");
  metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  EXPECT_THAT(metadata, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("has type JSON")));
}

TEST_F(PgDataTypeIntegrationTest, JsonIndexAndPrimaryKey) {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  // Verify that a JSONB column cannot be used as an index.
  std::vector<std::string> statements;
  statements.emplace_back(R"""(
        CREATE INDEX DataTypesByJsonValue
          ON DataTypes(JsonValue)
      )""");
  auto metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  if (UsingEmulator()) {
    EXPECT_THAT(metadata, StatusIs(StatusCode::kFailedPrecondition,
                                   HasSubstr("Cannot reference PG.JSONB")));
  } else {
    EXPECT_THAT(metadata, StatusIs(StatusCode::kFailedPrecondition,
                                   HasSubstr("unsupported type PG.JSONB")));
  }

  // Verify that a JSONB column cannot be used as a primary key.
  statements.clear();
  statements.emplace_back(R"""(
        CREATE TABLE JsonKey (
          Key JSONB NOT NULL,
          PRIMARY KEY (Key)
        )
      )""");
  metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  EXPECT_THAT(metadata, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("has type PG.JSONB")));
}

TEST_F(PgDataTypeIntegrationTest, InsertAndQueryWithJson) {
  auto& client = *client_;
  auto commit_result =
      client.Commit([&client](Transaction const& txn) -> StatusOr<Mutations> {
        auto dml_result = client.ExecuteDml(
            txn,
            SqlStatement("INSERT INTO DataTypes (Id, JsonValue)"
                         "  VALUES($1, $2)",
                         {{"p1", Value("Id-1")}, {"p2", Value(JsonB("42"))}}));
        if (!dml_result) return dml_result.status();
        return Mutations{};
      });
  ASSERT_STATUS_OK(commit_result);

  auto rows =
      client_->ExecuteQuery(SqlStatement("SELECT Id, JsonValue FROM DataTypes"
                                         "  WHERE Id = $1",
                                         {{"p1", Value("Id-1")}}));
  using RowType = std::tuple<std::string, absl::optional<JsonB>>;
  auto row = GetSingularRow(StreamOf<RowType>(rows));
  ASSERT_STATUS_OK(row);

  auto const& v = std::get<1>(*row);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, JsonB("42"));
}

TEST_F(DataTypeIntegrationTest, InsertAndQueryWithNumericKey) {
  auto& client = *client_;
  auto const key = MakeNumeric(42).value();

  auto commit_result = client.Commit(
      Mutations{InsertOrUpdateMutationBuilder("NumericKey", {"Key"})
                    .EmplaceRow(key)
                    .Build()});
  ASSERT_STATUS_OK(commit_result);

  auto rows = client.Read("NumericKey", KeySet::All(), {"Key"});
  using RowType = std::tuple<Numeric>;
  auto row = GetSingularRow(StreamOf<RowType>(rows));
  ASSERT_STATUS_OK(row);
  EXPECT_EQ(std::get<0>(*std::move(row)), key);
}

TEST_F(PgDataTypeIntegrationTest, NumericPrimaryKey) {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  // Verify that a NUMERIC column cannot be used as a primary key.
  std::vector<std::string> statements;
  statements.emplace_back(R"sql(
        CREATE TABLE NumericKey (
          Key NUMERIC NOT NULL,
          PRIMARY KEY(Key)
        )
      )sql");
  auto metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  EXPECT_THAT(metadata, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("has type PG.NUMERIC")));
}

TEST_F(DataTypeIntegrationTest, InsertAndQueryWithProtoEnumKey) {
  auto& client = *client_;
  auto const key = testing::Genre::POP;

  auto commit_result = client.Commit(
      Mutations{InsertOrUpdateMutationBuilder("ProtoEnumKey", {"Key"})
                    .EmplaceRow(key)
                    .Build()});
  if (UsingEmulator()) {
    EXPECT_THAT(commit_result, StatusIs(StatusCode::kNotFound));
  } else {
    ASSERT_STATUS_OK(commit_result);
    auto rows = client.Read("ProtoEnumKey", KeySet::All(), {"Key"});
    using RowType = std::tuple<ProtoEnum<testing::Genre>>;
    auto row = GetSingularRow(StreamOf<RowType>(rows));
    ASSERT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*std::move(row)), key);
  }
}

TEST_F(DataTypeIntegrationTest, DmlReturning) {
  auto& client = *client_;
  using RowType = std::tuple<std::string, std::int64_t>;

  std::vector<RowType> insert_actual;
  auto insert_result = client.Commit(
      [&client, &insert_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            INSERT INTO DataTypes (Id, Int64Value)
              VALUES ('Id-Ret-1', 1),
                     ('Id-Ret-2', 2),
                     ('Id-Ret-3', 3),
                     ('Id-Ret-4', 4)
              THEN RETURN Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        insert_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) insert_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(insert_result, IsOk());
  EXPECT_THAT(insert_actual,
              ElementsAre(RowType{"Id-Ret-1", 1}, RowType{"Id-Ret-2", 2},
                          RowType{"Id-Ret-3", 3}, RowType{"Id-Ret-4", 4}));

  std::vector<RowType> update_actual;
  auto update_result = client.Commit(
      [&client, &update_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            UPDATE DataTypes SET Int64Value = 100
              WHERE Id LIKE 'Id-Ret-%%'
              THEN RETURN Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        update_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) update_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(update_result, IsOk());
  EXPECT_THAT(update_actual,
              ElementsAre(RowType{"Id-Ret-1", 100}, RowType{"Id-Ret-2", 100},
                          RowType{"Id-Ret-3", 100}, RowType{"Id-Ret-4", 100}));

  std::vector<RowType> delete_actual;
  auto delete_result = client.Commit(
      [&client, &delete_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            DELETE FROM DataTypes
              WHERE Id LIKE 'Id-Ret-%%'
              THEN RETURN Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        delete_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) delete_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(delete_result, IsOk());
  EXPECT_THAT(delete_actual,
              ElementsAre(RowType{"Id-Ret-1", 100}, RowType{"Id-Ret-2", 100},
                          RowType{"Id-Ret-3", 100}, RowType{"Id-Ret-4", 100}));
}

TEST_F(PgDataTypeIntegrationTest, DmlReturning) {
  auto& client = *client_;
  using RowType = std::tuple<std::string, std::int64_t>;

  std::vector<RowType> insert_actual;
  auto insert_result = client.Commit(
      [&client, &insert_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            INSERT INTO DataTypes (Id, Int64Value)
              VALUES ('Id-Ret-1', 1),
                     ('Id-Ret-2', 2),
                     ('Id-Ret-3', 3),
                     ('Id-Ret-4', 4)
              RETURNING Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        insert_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) insert_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(insert_result, IsOk());
  EXPECT_THAT(insert_actual,
              ElementsAre(RowType{"Id-Ret-1", 1}, RowType{"Id-Ret-2", 2},
                          RowType{"Id-Ret-3", 3}, RowType{"Id-Ret-4", 4}));

  std::vector<RowType> update_actual;
  auto update_result = client.Commit(
      [&client, &update_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            UPDATE DataTypes SET Int64Value = 100
              WHERE Id LIKE 'Id-Ret-%%'
              RETURNING Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        update_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) update_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(update_result, IsOk());
  EXPECT_THAT(update_actual,
              ElementsAre(RowType{"Id-Ret-1", 100}, RowType{"Id-Ret-2", 100},
                          RowType{"Id-Ret-3", 100}, RowType{"Id-Ret-4", 100}));

  std::vector<RowType> delete_actual;
  auto delete_result = client.Commit(
      [&client, &delete_actual](Transaction const& txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(R"""(
            DELETE FROM DataTypes
              WHERE Id LIKE 'Id-Ret-%%'
              RETURNING Id, Int64Value
        )""");
        auto rows = client.ExecuteQuery(txn, std::move(sql));
        EXPECT_EQ(rows.RowsModified(), UsingEmulator() ? 0 : 4);
        delete_actual.clear();  // may be a re-run
        for (auto& row : StreamOf<RowType>(rows)) {
          if (row) delete_actual.push_back(*std::move(row));
        }
        return Mutations{};
      });
  ASSERT_THAT(delete_result, IsOk());
  EXPECT_THAT(delete_actual,
              ElementsAre(RowType{"Id-Ret-1", 100}, RowType{"Id-Ret-2", 100},
                          RowType{"Id-Ret-3", 100}, RowType{"Id-Ret-4", 100}));
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
                "  VALUES(@id, @struct.StringValue, @struct.ArrayInt64Value)",
                {{"id", Value("Id-1")}, {"struct", Value(data)}}));
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
  EXPECT_THAT(v, ElementsAre(data));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
