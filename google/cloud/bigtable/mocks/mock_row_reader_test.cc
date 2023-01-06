// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::Row;
using ::google::cloud::testing_util::StatusIs;

struct Result {
  explicit Result(std::vector<Row> const& in_rows, Status in_final_status)
      : final_status(std::move(in_final_status)) {
    rows.reserve(in_rows.size());
    for (auto const& r : in_rows) rows.emplace_back(r.row_key());
  }

  explicit Result(bigtable::RowReader reader) {
    for (auto const& sor : reader) {
      if (!sor) {
        final_status = std::move(sor).status();
      } else {
        rows.emplace_back((*std::move(sor)).row_key());
      }
    }
  }

  std::vector<std::string> rows;
  Status final_status;
};

TEST(MakeRowReaderTest, Empty) {
  std::vector<Row> rows;
  Status final_status;

  auto reader = MakeRowReader(rows);

  auto actual = Result(std::move(reader));
  auto expected = Result(rows, final_status);
  EXPECT_EQ(actual.rows, expected.rows);
  EXPECT_EQ(actual.final_status, expected.final_status);
}

TEST(MakeRowReaderTest, Rows) {
  std::vector<Row> rows = {Row("r1", {}), Row("r2", {})};
  Status final_status;

  auto reader = MakeRowReader(rows);

  auto actual = Result(std::move(reader));
  auto expected = Result(rows, final_status);
  EXPECT_EQ(actual.rows, expected.rows);
  EXPECT_EQ(actual.final_status, expected.final_status);
}

TEST(MakeRowReaderTest, StatusOnly) {
  std::vector<Row> rows;
  auto final_status = Status(StatusCode::kPermissionDenied, "fail");

  auto reader = MakeRowReader(rows, final_status);

  auto actual = Result(std::move(reader));
  auto expected = Result(rows, final_status);
  EXPECT_EQ(actual.rows, expected.rows);
  EXPECT_EQ(actual.final_status, expected.final_status);
}

TEST(MakeRowReaderTest, RowsThenStatus) {
  std::vector<Row> rows = {Row("r1", {}), Row("r2", {})};
  auto final_status = Status(StatusCode::kPermissionDenied, "fail");

  auto reader = MakeRowReader(rows, final_status);

  auto actual = Result(std::move(reader));
  auto expected = Result(rows, final_status);
  EXPECT_EQ(actual.rows, expected.rows);
  EXPECT_EQ(actual.final_status, expected.final_status);
}

TEST(MakeRowReaderTest, CancelEndsGoodStream) {
  std::vector<Row> rows = {Row("r1", {}), Row("r2", {})};
  Status final_status;

  auto reader = MakeRowReader(rows);

  auto it = reader.begin();
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");

  // Cancel the reader
  reader.Cancel();

  // Verify that the next iterator points to `end()` instead of "r2"
  EXPECT_EQ(++it, reader.end());

  // Verify that new iterators point to `end()`
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST(MakeRowReaderTest, CancelEndsBadStream) {
  std::vector<Row> rows = {Row("r1", {}), Row("r2", {})};
  auto final_status = Status(StatusCode::kCancelled, "cancelled");

  auto reader = MakeRowReader(rows, final_status);

  auto it = reader.begin();
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");

  // Cancel the reader
  reader.Cancel();

  // Verify that the next iterator points to `final_status` instead of "r2"
  EXPECT_THAT(*++it, StatusIs(StatusCode::kCancelled));

  // Verify that the next iterator points to `end()`
  EXPECT_EQ(++it, reader.end());

  // Verify that new iterators return the `final_status`
  EXPECT_THAT(*reader.begin(), StatusIs(StatusCode::kCancelled));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_mocks
}  // namespace cloud
}  // namespace google
