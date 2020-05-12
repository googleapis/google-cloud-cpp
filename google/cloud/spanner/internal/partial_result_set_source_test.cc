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

#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/mock_partial_result_set_reader.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

namespace spanner_proto = ::google::spanner::v1;

using ::google::cloud::spanner_testing::IsProtoEqual;
using ::google::cloud::spanner_testing::MockPartialResultSetReader;
using ::google::protobuf::TextFormat;
using ::testing::HasSubstr;
using ::testing::Return;

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<Row> contains the given Row") {
  return arg && *arg == expected;
}

/// @test Verify the behavior when the initial `Read()` fails.
TEST(PartialResultSetSourceTest, InitialReadFailure) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  Status finish_status(StatusCode::kInvalidArgument, "invalid");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));

  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_FALSE(reader.status().ok());
  EXPECT_EQ(reader.status().code(), StatusCode::kInvalidArgument);
}

/**
 * @test Verify the behavior when we have a successful `Read()` followed by a
 * failed `Read()`.
 */
TEST(PartialResultSetSourceTest, ReadSuccessThenFailure) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  Status finish_status(StatusCode::kCancelled, "cancelled");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));
  // The first call to NextRow() yields a row but the second gives an error.
  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());
  EXPECT_THAT((*reader)->NextRow(),
              IsValidAndEquals(MakeTestRow({{"AnInt", Value(80)}})));
  auto row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kCancelled);
}

/// @test Verify the behavior when the first response does not contain metadata.
TEST(PartialResultSetSourceTest, MissingMetadata) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  spanner_proto::PartialResultSet response;
  EXPECT_CALL(*grpc_reader, Read()).WillOnce(Return(response));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));
  // The destructor should try to cancel the RPC to avoid deadlocks.
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(1);

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_FALSE(reader.status().ok());
  EXPECT_EQ(reader.status().code(), StatusCode::kInternal);
  EXPECT_EQ(reader.status().message(), "response contained no metadata");
}

/**
 * @test Verify the behavior when the response does not contain data nor row
 * type information.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeNoData) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  auto constexpr kText = R"pb(metadata: {})pb";
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(Row{}));
}

/**
 * @test Verify the behavior when the received metadata contains data but not
 * row type information.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeWithData) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
  auto constexpr kText = R"pb(
    metadata: {}
    values: { string_value: "10" })pb";
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*grpc_reader, Read()).WillOnce(Return(response));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));
  // The destructor should try to cancel the RPC to avoid deadlocks.
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(1);

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  StatusOr<Row> row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kInternal);
  EXPECT_THAT(row.status().message(),
              HasSubstr("missing row type information"));
}

/**
 * @test Verify the functionality of the PartialResultSetSource, including
 * properly handling metadata, stats, and data values.
 * All of the data is returned in a single Read() from the gRPC reader.
 */
TEST(PartialResultSetSourceTest, SingleResponse) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

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
  spanner_proto::ResultSetMetadata expected_metadata;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextExpectedMetadata, &expected_metadata));
  auto actual_metadata = (*reader)->Metadata();
  EXPECT_TRUE(actual_metadata.has_value());
  EXPECT_THAT(*actual_metadata, IsProtoEqual(expected_metadata));

  // Verify the returned rows are correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(10)},
                                        {"UserName", Value("user10")},
                                    })));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(Row{}));

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
  spanner_proto::ResultSetStats expected_stats;
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
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
        values: { string_value: "99user99" }
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
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(response[2]))
      .WillOnce(Return(response[3]))
      .WillOnce(Return(response[4]))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned rows are correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(10)},
                                        {"UserName", Value("user10")},
                                    })));
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(22)},
                                        {"UserName", Value("user22")},
                                    })));
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(99)},
                                        {"UserName", Value("99user99")},
                                    })));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(Row{}));
}

/**
 * @test Verify the behavior when a response with no values is received.
 */
TEST(PartialResultSetSourceTest, ResponseWithNoValues) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
      "",
      R"pb(
        values: { string_value: "22" }
      )pb",
  }};
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(response[2]))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned row is correct.
  EXPECT_THAT((*reader)->NextRow(),
              IsValidAndEquals(MakeTestRow({{"UserId", Value(22)}})));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(Row{}));
}

/**
 * @Test Verify reassembling chunked values works correctly, including a mixture
 * of chunked and unchunked values.
 */
TEST(PartialResultSetSourceTest, ChunkedStringValueWellFormed) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(response[2]))
      .WillOnce(Return(response[3]))
      .WillOnce(Return(response[4]))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned values are correct.
  for (const auto& value :
       {"not_chunked", "first_chunksecond_chunkthird_chunk",
        "second group first_chunk second group second_chunk",
        "also not_chunked", "still not_chunked"}) {
    EXPECT_THAT((*reader)->NextRow(),
                IsValidAndEquals(MakeTestRow({{"Prose", Value(value)}})));
  }

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(Row{}));
}

/**
 * @test Verify the behavior when `chunked_value` is set but there are no
 * values in the response.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetNoValue) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
      R"pb(chunked_value: true)pb",
  }};
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));
  // The destructor should try to cancel the RPC to avoid deadlocks.
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(1);

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next row should fail.
  auto row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kInternal);
  EXPECT_EQ(
      row.status().message(),
      "PartialResultSet had chunked_value set true but contained no values");
}

/**
 * @test Verify the behavior when a response with no values follows one
 * with `chunked_value` set.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetNoFollowingValue) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
      R"pb()pb",
  }};
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));
  // The destructor should try to cancel the RPC to avoid deadlocks.
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(1);

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next row should fail.
  auto row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kInternal);
  EXPECT_EQ(row.status().message(),
            "PartialResultSet contained no values to merge with prior "
            "chunked_value");
}

/**
 * @test Verify the behavior when `chunked_value` is set in the final response.
 */
TEST(PartialResultSetSourceTest, ChunkedValueSetAtEndOfStream) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next row should fail.
  auto row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kInternal);
  EXPECT_EQ(row.status().message(),
            "incomplete chunked_value at end of stream");
}

/**
 * @test Verify the behavior when attempting to merge a value that can't be
 * chunked (float64/number).
 */
TEST(PartialResultSetSourceTest, ChunkedValueMergeFailure) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(response[2]));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));
  // The destructor should try to cancel the RPC to avoid deadlocks.
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(1);

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next row should fail.
  auto row = (*reader)->NextRow();
  EXPECT_EQ(row.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(row.status().message(), "invalid type");
}

/**
 * @test Verify the behavior when we get an incomplete Row.
 */
TEST(PartialResultSetSourceTest, ErrorOnIncompleteRow) {
  auto grpc_reader = absl::make_unique<MockPartialResultSetReader>();
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
  std::array<spanner_proto::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }
  EXPECT_CALL(*grpc_reader, Read())
      .WillOnce(Return(response[0]))
      .WillOnce(Return(response[1]))
      .WillOnce(Return(response[2]))
      .WillOnce(Return(response[3]))
      .WillOnce(Return(response[4]))
      .WillOnce(Return(optional<spanner_proto::PartialResultSet>{}));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(Status()));

  auto context = absl::make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetSource::Create(std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the first two rows are correct.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(10)},
                                        {"UserName", Value("user10")},
                                    })));
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(MakeTestRow({
                                        {"UserId", Value(22)},
                                        {"UserName", Value("user22")},
                                    })));

  auto row = (*reader)->NextRow();
  EXPECT_FALSE(row.ok());
  EXPECT_EQ(row.status().code(), StatusCode::kInternal);
  EXPECT_THAT(row.status().message(), HasSubstr("incomplete row"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
