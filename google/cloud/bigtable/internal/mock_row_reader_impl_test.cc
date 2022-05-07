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

#include "google/cloud/bigtable/internal/mock_row_reader_impl.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::Row;
using ::google::cloud::bigtable::RowReader;
using ::testing::ElementsAreArray;

struct Result {
  explicit Result(StatusOr<Row> const& in) {
    if (in.ok()) {
      row = in.value().row_key();
    } else {
      status = in.status();
    }
  }

  Status status;
  std::string row;
};

bool operator==(Result const& a, Result const& b) {
  return a.status == b.status && a.row == b.row;
}

std::ostream& operator<<(std::ostream& os, Result const& a) {
  if (!a.status.ok()) return os << "Status: " << a.status;
  return os << "Row: " << a.row;
}

template <typename T>
std::vector<Result> ToResult(T& rs) {
  std::vector<Result> ret;
  // NOLINTNEXTLINE(performance-inefficient-vector-operation)
  for (auto const& r : rs) ret.emplace_back(r);
  return ret;
}

TEST(MockRowReaderTest, Empty) {
  std::vector<StatusOr<Row>> rows;

  auto reader = MakeMockRowReader(rows);

  auto actual = ToResult(reader);
  auto expected = ToResult(rows);
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(MockRowReaderTest, Rows) {
  std::vector<StatusOr<Row>> rows = {Row("r1", {}), Row("r2", {})};

  auto reader = MakeMockRowReader(rows);

  auto actual = ToResult(reader);
  auto expected = ToResult(rows);
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(MockRowReaderTest, StatusOnly) {
  std::vector<StatusOr<Row>> rows = {
      Status(StatusCode::kPermissionDenied, "fail")};

  auto reader = MakeMockRowReader(rows);

  auto actual = ToResult(reader);
  auto expected = ToResult(rows);
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(MockRowReaderTest, RowsThenStatus) {
  std::vector<StatusOr<Row>> rows = {
      Row("r1", {}), Row("r2", {}),
      Status(StatusCode::kPermissionDenied, "fail")};

  auto reader = MakeMockRowReader(rows);

  auto actual = ToResult(reader);
  auto expected = ToResult(rows);
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(MockRowReaderTest, ReaderEndsOnStatus) {
  std::vector<StatusOr<Row>> rows = {
      Status(StatusCode::kPermissionDenied, "fail"), Row("ignored", {})};
  std::vector<StatusOr<Row>> expected_rows = {
      Status(StatusCode::kPermissionDenied, "fail")};

  auto reader = MakeMockRowReader(rows);

  auto actual = ToResult(reader);
  auto expected = ToResult(expected_rows);
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(MockRowReaderTest, CancelEndsStream) {
  std::vector<StatusOr<Row>> rows = {Row("r1", {}), Row("r2", {})};

  auto reader = MakeMockRowReader(rows);

  auto it = reader.begin();
  EXPECT_EQ(Result(*it), Result(rows[0]));

  // Cancel the reader
  reader.Cancel();

  // Increment the iterator. Verify that it points to `end()` instead of "r2"
  it++;
  EXPECT_EQ(it, reader.end());

  // Verify that new iterators are invalidated too.
  EXPECT_EQ(reader.begin(), reader.end());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
