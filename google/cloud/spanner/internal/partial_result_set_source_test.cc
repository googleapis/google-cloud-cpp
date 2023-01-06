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

#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/testing/mock_partial_result_set_reader.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_testing::MockPartialResultSetReader;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
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
StatusOr<std::unique_ptr<ResultSourceInterface>> CreatePartialResultSetSource(
    std::unique_ptr<PartialResultSetReader> reader, Options opts = {}) {
  internal::OptionsSpan span(internal::MergeOptions(
      std::move(opts.set<StringOption>(CurrentTestName())),
      internal::CurrentOptions()));
  return PartialResultSetSource::Create(std::move(reader));
}

// Returns a functor that expects the current `StringOption` to match the test
// name.
std::function<void()> VoidMock() {
  return [] {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
  };
}

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

absl::optional<PartialResultSet> ReadResult(
    google::spanner::v1::PartialResultSet response) {
  return PartialResultSet{std::move(response), false};
}

absl::optional<PartialResultSet> ReadResult() { return {}; }

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<Row> contains the given Row") {
  return arg && *arg == expected;
}

/// @test Verify the behavior when the initial `Read()` fails.
TEST(PartialResultSetSourceTest, InitialReadFailure) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_)).WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish())
      .WillOnce(ResultMock(Status(StatusCode::kInvalidArgument, "invalid")));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  EXPECT_THAT(reader, StatusIs(StatusCode::kInvalidArgument, "invalid"));
}

/**
 * @test Verify the behavior when we have a successful `Read()` followed by a
 * failed `Read()`.
 */
TEST(PartialResultSetSourceTest, ReadSuccessThenFailure) {
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "AnInt",
          type: { code: INT64 }
        }
      }
    }
    values: { string_value: "80" }
  )pb";
  google::spanner::v1::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response)))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish())
      .WillOnce(ResultMock(Status(StatusCode::kCancelled, "cancelled")));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  // The first call to NextRow() yields a row but the second gives an error.
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow(
                                        {{"AnInt", spanner::Value(80)}})));
  auto row = (*reader)->NextRow();
  EXPECT_THAT(row, StatusIs(StatusCode::kCancelled, "cancelled"));
}

/// @test Verify the behavior when the first response does not contain metadata.
TEST(PartialResultSetSourceTest, MissingMetadata) {
  google::spanner::v1::PartialResultSet response;

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_)).WillOnce(ResultMock(ReadResult(response)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).WillOnce(VoidMock());

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  EXPECT_THAT(reader, StatusIs(StatusCode::kInternal,
                               "PartialResultSetSource response"
                               " contained no metadata"));
}

/**
 * @test Verify the behavior when the response does not contain data nor row
 * type information.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeNoData) {
  auto constexpr kText = R"pb(
    metadata: {}
  )pb";
  google::spanner::v1::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response)))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

/**
 * @test Verify the behavior when the received metadata contains data but not
 * row type information.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeWithData) {
  auto constexpr kText = R"pb(
    metadata: {}
    values: { string_value: "10" }
  )pb";
  google::spanner::v1::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response)))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kInternal,
                       "PartialResultSetSource metadata is missing row type"));
}

/**
 * @test Verify the functionality of the PartialResultSetSource, including
 * properly handling metadata, stats, and data values.
 * All of the data is returned in a single Read() from the gRPC reader.
 */
TEST(PartialResultSetSourceTest, SingleResponse) {
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "UserId",
          type: { code: INT64 }
        }
        fields: {
          name: "UserName",
          type: { code: STRING }
        }
      }
    }
    values: { string_value: "10" }
    values: { string_value: "user10" }
    stats: {
      query_stats: {
        fields: {
          key: "rows_returned",
          value: { string_value: "1" }
        }
        fields: {
          key: "elapsed_time",
          value: { string_value: "1.22 secs" }
        }
        fields: {
          key: "cpu_time",
          value: { string_value: "1.19 secs" }
        }
      }
    }
  )pb";
  google::spanner::v1::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response)))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned metadata is correct.
  auto constexpr kTextExpectedMetadata = R"pb(
    row_type: {
      fields: {
        name: "UserId",
        type: { code: INT64 }
      }
      fields: {
        name: "UserName",
        type: { code: STRING }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata expected_metadata;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextExpectedMetadata, &expected_metadata));
  auto actual_metadata = (*reader)->Metadata();
  EXPECT_TRUE(actual_metadata.has_value());
  EXPECT_THAT(*actual_metadata, IsProtoEqual(expected_metadata));

  // Verify the returned rows are correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                        {"UserId", spanner::Value(10)},
                                        {"UserName", spanner::Value("user10")},
                                    })));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));

  // Verify the returned stats are correct.
  auto constexpr kTextExpectedStats = R"pb(
    query_stats: {
      fields: {
        key: "rows_returned",
        value: { string_value: "1" }
      }
      fields: {
        key: "elapsed_time",
        value: { string_value: "1.22 secs" }
      }
      fields: {
        key: "cpu_time",
        value: { string_value: "1.19 secs" }
      }
    }
  )pb";
  google::spanner::v1::ResultSetStats expected_stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedStats, &expected_stats));
  auto actual_stats = (*reader)->Stats();
  EXPECT_TRUE(actual_stats.has_value());
  EXPECT_THAT(*actual_stats, IsProtoEqual(expected_stats));
}

/**
 * @test Verify the functionality of the PartialResultSetSource when the gRPC
 * reader returns data across multiple Read() calls.
 */
TEST(PartialResultSetSourceTest, MultipleResponses) {
  std::array<char const*, 5> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "UserId",
              type: { code: INT64 }
            }
            fields: {
              name: "UserName",
              type: { code: STRING }
            }
          }
        }
      )pb",
      R"pb(
        values: { string_value: "10" }
        values: { string_value: "user10" }
      )pb",
      R"pb(
        values: { string_value: "22" }
        values: { string_value: "user22" }
      )pb",
      R"pb(
        values: { string_value: "99" }
      )pb",
      R"pb(
        values: { string_value: "user99" }
        stats: {
          query_stats: {
            fields: {
              key: "rows_returned",
              value: { string_value: "3" }
            }
            fields: {
              key: "elapsed_time",
              value: { string_value: "4.22 secs" }
            }
            fields: {
              key: "cpu_time",
              value: { string_value: "3.19 secs" }
            }
          }
        }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  // Choose values for the `StreamingResumabilityBufferSizeOption` that,
  // given `text`, yield complete rows after varying numbers of calls to
  // `ReadFromStream()`.
  for (std::size_t buffer_size : {0, 153, 161, 321, 385, 448}) {
    auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
    EXPECT_CALL(*grpc_reader, Read(_))
        .WillOnce(ResultMock(ReadResult(response[0])))
        .WillOnce(ResultMock(ReadResult(response[1])))
        .WillOnce(ResultMock(ReadResult(response[2])))
        .WillOnce(ResultMock(ReadResult(response[3])))
        .WillOnce(ResultMock(ReadResult(response[4])))
        .WillOnce(ResultMock(ReadResult()));
    EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
    EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    auto reader = CreatePartialResultSetSource(
        std::move(grpc_reader),
        Options{}.set<spanner::StreamingResumabilityBufferSizeOption>(
            buffer_size));
    ASSERT_STATUS_OK(reader);

    // Verify the returned rows are correct.
    EXPECT_THAT((*reader)->NextRow(),
                IsValidAndEquals(spanner_mocks::MakeRow({
                    {"UserId", spanner::Value(10)},
                    {"UserName", spanner::Value("user10")},
                })));
    EXPECT_THAT((*reader)->NextRow(),
                IsValidAndEquals(spanner_mocks::MakeRow({
                    {"UserId", spanner::Value(22)},
                    {"UserName", spanner::Value("user22")},
                })));
    EXPECT_THAT((*reader)->NextRow(),
                IsValidAndEquals(spanner_mocks::MakeRow({
                    {"UserId", spanner::Value(99)},
                    {"UserName", spanner::Value("user99")},
                })));

    // At end of stream, we get an 'ok' response with an empty row.
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
  }
}

/**
 * @test Verify the behavior when a response with no values is received.
 */
TEST(PartialResultSetSourceTest, ResponseWithNoValues) {
  std::array<char const*, 3> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "UserId",
              type: { code: INT64 }
            }
          }
        }
      )pb",
      R"pb(
      )pb",
      R"pb(
        values: { string_value: "22" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult(response[2])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned row is correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow(
                                        {{"UserId", spanner::Value(22)}})));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

/**
 * @test Verify reassembling chunked values works correctly, including a mixture
 * of chunked and un-chunked values.
 */
TEST(PartialResultSetSourceTest, ChunkedStringValueWellFormed) {
  std::array<char const*, 5> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "Prose",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "not_chunked" }
        values: { string_value: "first_chunk" }
        chunked_value: true
      )pb",
      // Note this is part of a value that spans 3 responses.
      R"pb(
        values: { string_value: "second_chunk" }
        chunked_value: true
      )pb",
      R"pb(
        values: { string_value: "third_chunk" }
        values: { string_value: "second group first_chunk " }
        chunked_value: true
      )pb",
      R"pb(
        values: { string_value: "second group second_chunk" }
        values: { string_value: "also not_chunked" }
      )pb",
      R"pb(
        values: { string_value: "still not_chunked" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult(response[2])))
      .WillOnce(ResultMock(ReadResult(response[3])))
      .WillOnce(ResultMock(ReadResult(response[4])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned values are correct.
  for (auto const& value :
       {"not_chunked", "first_chunksecond_chunkthird_chunk",
        "second group first_chunk second group second_chunk",
        "also not_chunked", "still not_chunked"}) {
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow(
                                          {{"Prose", spanner::Value(value)}})));
  }

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

/**
 * @test Verify the behavior when `chunked_value` is set but there are no
 * values in the response.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetNoValue) {
  std::array<char const*, 2> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "Prose",
              type: { code: STRING }
            }
          }
        }
      )pb",
      R"pb(
        chunked_value: true
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // We ignore `chunked_value` when there is no value to be chunked.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

/**
 * @test Verify the behavior when a response with no values follows one
 * with `chunked_value` set.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetNoFollowingValue) {
  std::array<char const*, 2> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "Prose",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "incomplete" }
        chunked_value: true
      )pb",
      R"pb(
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Trying to read the next row should fail.
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kInternal,
                       "PartialResultSetSource stream ended at a point"
                       " that is not on a row boundary"));
}

/**
 * @test Verify the behavior when `chunked_value` is set in the final response.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetAtEndOfStream) {
  std::array<char const*, 2> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "Prose",
              type: { code: STRING }
            }
          }
        }
      )pb",
      R"pb(
        values: { string_value: "incomplete" }
        chunked_value: true
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Trying to read the next row should fail.
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kInternal,
                       "PartialResultSetSource stream ended at a point"
                       " that is not on a row boundary"));
}

/**
 * @test Verify the behavior when attempting to merge a value that can't be
 * chunked (float64/number).
 */
TEST(PartialResultSetSourceTest, ChunkedValueMergeFailure) {
  std::array<char const*, 3> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "Number",
              type: { code: FLOAT64 }
            }
          }
        }
      )pb",
      R"pb(
        values: { number_value: 86 }
        chunked_value: true
      )pb",
      R"pb(
        values: { number_value: 99 }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult(response[2])));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).WillOnce(VoidMock());

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Trying to read the next row should fail.
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kInvalidArgument, "invalid type"));
}

/**
 * @test Verify the behavior when we get an incomplete Row.
 */
TEST(PartialResultSetSourceTest, ErrorOnIncompleteRow) {
  std::array<char const*, 5> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "UserId",
              type: { code: INT64 }
            }
            fields: {
              name: "UserName",
              type: { code: STRING }
            }
          }
        }
      )pb",
      R"pb(
        values: { string_value: "10" }
        values: { string_value: "user10" }
      )pb",
      R"pb(
        values: { string_value: "22" }
        values: { string_value: "user22" }
      )pb",
      R"pb(
        values: { string_value: "99" }
      )pb",
      R"pb(
        stats: {
          query_stats: {
            fields: {
              key: "rows_returned",
              value: { string_value: "3" }
            }
            fields: {
              key: "elapsed_time",
              value: { string_value: "4.22 secs" }
            }
            fields: {
              key: "cpu_time",
              value: { string_value: "3.19 secs" }
            }
          }
        }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(ResultMock(ReadResult(response[0])))
      .WillOnce(ResultMock(ReadResult(response[1])))
      .WillOnce(ResultMock(ReadResult(response[2])))
      .WillOnce(ResultMock(ReadResult(response[3])))
      .WillOnce(ResultMock(ReadResult(response[4])))
      .WillOnce(ResultMock(ReadResult()));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the first two rows are correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                        {"UserId", spanner::Value(10)},
                                        {"UserName", spanner::Value("user10")},
                                    })));
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                        {"UserId", spanner::Value(22)},
                                        {"UserName", spanner::Value("user22")},
                                    })));

  // Trying to read the next row should fail.
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kInternal,
                       "PartialResultSetSource stream ended at a point"
                       " that is not on a row boundary"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
