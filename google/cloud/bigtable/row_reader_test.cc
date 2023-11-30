// Copyright 2017 Google LLC
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

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

TEST(RowReaderTest, DefaultConstructor) {
  RowReader reader;
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST(RowReaderTest, BadStatusOnly) {
  auto impl = std::make_shared<bigtable_internal::StatusOnlyRowReader>(
      Status(StatusCode::kUnimplemented, "unimplemented"));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnimplemented));
  EXPECT_EQ(++it, reader.end());
}

TEST(RowReaderTest, IteratorPostincrement) {
  std::vector<Row> rows = {Row("r1", {})};

  auto reader = bigtable_mocks::MakeRowReader(rows);

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  // This postincrement is what we are testing
  auto it2 = it++;
  ASSERT_STATUS_OK(*it2);
  EXPECT_EQ((*it2)->row_key(), "r1");
  EXPECT_EQ(it, reader.end());
}

class MockRowReader : public bigtable_internal::RowReaderImpl {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, bigtable::Row>), Advance, (), (override));
};

TEST(RowReaderTest, OptionsSpan) {
  struct TestOption {
    using Type = std::string;
  };

  auto mock = std::make_shared<MockRowReader>();

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Advance).Times(3).WillRepeatedly([]() {
    // Verify that the OptionsSpan from construction applies for each Advance.
    EXPECT_TRUE(google::cloud::internal::CurrentOptions().has<TestOption>());
    return Row("row", {});
  });
  EXPECT_CALL(*mock, Advance).WillOnce(Return(Status()));

  // Construct a RowReader with an active OptionsSpan.
  google::cloud::internal::OptionsSpan span(Options{}.set<TestOption>("set"));
  auto reader = bigtable_internal::MakeRowReader(std::move(mock));

  // Clear the OptionsSpan before we call begin().
  google::cloud::internal::OptionsSpan overlay(Options{});
  for (auto const& sor : reader) {
    EXPECT_STATUS_OK(sor);
  }
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::IsActive;

TEST(RowReader, CallSpanActiveThroughout) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = google::cloud::internal::MakeSpan("call span");
  auto mock = std::make_shared<MockRowReader>();

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Advance).Times(3).WillRepeatedly([span] {
    // Verify that the active span from construction applies for each Advance.
    EXPECT_THAT(span, IsActive());
    return Row("row", {});
  });
  EXPECT_CALL(*mock, Advance).WillOnce(Return(Status()));

  auto reader = [span, mock] {
    // Set "call span" as active.
    google::cloud::internal::OTelScope scope(span);
    return bigtable_internal::MakeRowReader(mock);
  }();

  // Clear the active span before we iterate.
  auto overlay = google::cloud::internal::MakeSpan("overlay");
  auto scope = opentelemetry::trace::Scope(overlay);
  for (auto const& row : reader) {
    EXPECT_THAT(overlay, IsActive());
    EXPECT_STATUS_OK(row);
  }
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
