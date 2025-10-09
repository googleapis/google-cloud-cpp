
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

#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/testing/mock_partial_result_set_reader.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::Return;
using ::testing::UnitTest;

std::string CurrentTestName() {
  return UnitTest::GetInstance()->current_test_info()->name();
}

struct StringOption {
  using Type = std::string;
};

// Create the `PartialResultSetSource` within an `OptionsSpan` that has its
// `StringOption` set to the current test name, so that we might check that
// all `PartialResultSetReader` calls happen within a matching span.
StatusOr<std::unique_ptr<PartialResultSourceInterface>>
CreatePartialResultSetSource(std::unique_ptr<PartialResultSetReader> reader,
                             Options opts = {}) {
  internal::OptionsSpan span(internal::MergeOptions(
      std::move(opts.set<StringOption>(CurrentTestName())),
      internal::CurrentOptions()));
  return PartialResultSetSource::Create(std::move(reader));
}

// // Returns a functor that expects the current `StringOption` to match the
// test
// // name.
// std::function<void()> VoidMock() {
//   return [] {
//     EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
//               CurrentTestName());
//   };
// }

// Returns a functor that will return the argument after expecting that the
// current `StringOption` matches the test name.
template <typename T>
std::function<T()> ResultMock(T const& result) {
  return [result]() {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    return result;
  };
}

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<QueryRow> contains the given QueryRow") {
  return arg && *arg == expected;
}

/// @test Verify the behavior when the initial `Read()` fails.
TEST(PartialResultSetSourceTest, InitialReadFailure) {
  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([](absl::optional<std::string> const&,
                   UnownedPartialResultSet&) { return false; });
  EXPECT_CALL(*grpc_reader, Finish())
      .WillOnce(ResultMock(Status(StatusCode::kInvalidArgument, "invalid")));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  EXPECT_THAT(reader, StatusIs(StatusCode::kInvalidArgument, "invalid"));
}

/**
 * @test Verify the behavior when the response does not contain data.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeNoData) {
  auto constexpr kText = R"pb(
  )pb";
  google::bigtable::v2::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([&response](absl::optional<std::string> const&,
                            UnownedPartialResultSet& result) {
        result.result = response;
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(bigtable::QueryRow{}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
