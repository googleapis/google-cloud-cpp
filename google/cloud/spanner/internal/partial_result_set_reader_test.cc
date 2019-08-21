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

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
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

using ::google::cloud::internal::make_unique;
using ::google::cloud::spanner_testing::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;

/**
 * A gmock Matcher that verifies a value returned from PartialResultSetReader
 * is valid and equals an expected value.
 */
class ReaderValueMatcher
    : public testing::MatcherInterface<StatusOr<optional<Value>> const&> {
 public:
  explicit ReaderValueMatcher(Value expected)
      : expected_(std::move(expected)) {}

  bool MatchAndExplain(StatusOr<optional<Value>> const& actual,
                       testing::MatchResultListener* listener) const override {
    if (!actual.ok()) {
      *listener << "reader value is not ok: " << actual.status();
      return false;
    }
    if (!actual->has_value()) {
      *listener << "reader value is empty";
      return false;
    }
    if (**actual != expected_) {
      *listener << testing::PrintToString(**actual) << " does not equal "
                << testing::PrintToString(expected_);
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "is valid and equals " << testing::PrintToString(expected_);
  }

 private:
  Value expected_;
};

testing::Matcher<StatusOr<optional<Value>> const&> IsValidAndEquals(
    Value expected) {
  return testing::MakeMatcher(new ReaderValueMatcher(std::move(expected)));
}

class MockGrpcReader : public PartialResultSetReader::GrpcReader {
 public:
  MOCK_METHOD1(Read, bool(spanner_proto::PartialResultSet*));
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD0(WaitForInitialMetadata, void());
};

/// @test Verify the behavior when the initial `Read()` fails.
TEST(PartialResultSetReaderTest, InitialReadFailure) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  EXPECT_CALL(*grpc_reader, Read(_)).WillOnce(Return(false));
  grpc::Status finish_status(grpc::StatusCode::INVALID_ARGUMENT, "invalid");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_FALSE(reader.status().ok());
  EXPECT_EQ(reader.status().code(), StatusCode::kInvalidArgument);
}

/**
 * @test Verify the behavior when we have a successful `Read()` followed by a
 * failed `Read()`.
 */
TEST(PartialResultSetReaderTest, ReadSuccessThenFailure) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "AnInt",
              type: { code: INT64 }
            }
          }
        }
        values: { string_value: "80" }
      )pb",
      &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  grpc::Status finish_status(grpc::StatusCode::CANCELLED, "cancelled");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));
  // The first call to NextValue() yields a value but the second gives an error.
  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(80)));
  auto value = (*reader)->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kCancelled);
}

/// @test Verify the behavior when the first response does not contain metadata.
TEST(PartialResultSetReaderTest, MissingMetadata) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_FALSE(reader.status().ok());
  EXPECT_EQ(reader.status().code(), StatusCode::kInternal);
  EXPECT_EQ(reader.status().message(), "response contained no metadata");
}

/**
 * @test Verify the behavior when the response does not contain data nor row
 * type information.
 */
TEST(PartialResultSetReaderTest, MissingRowTypeNoData) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(metadata: {})pb", &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  StatusOr<optional<Value>> value = reader.value()->NextValue();
  EXPECT_STATUS_OK(value);
  EXPECT_FALSE(value->has_value());
}

/**
 * @test Verify the behavior when the received metadata contains data but not
 * row type information.
 */
TEST(PartialResultSetReaderTest, MissingRowTypeWithData) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            metadata: {}
                                            values: { string_value: "10" })pb",
                                          &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  StatusOr<optional<Value>> value = reader.value()->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kInternal);
  EXPECT_THAT(value.status().message(),
              HasSubstr("missing row type information"));
}

/**
 * @test Verify the functionality of the PartialResultSetReader, including
 * properly handling metadata, stats, and data values.
 * All of the data is returned in a single Read() from the gRPC reader.
 */
TEST(PartialResultSetReaderTest, SingleResponse) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      )pb",
      &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned metadata is correct.
  spanner_proto::ResultSetMetadata expected_metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
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
      )pb",
      &expected_metadata));
  auto actual_metadata = (*reader)->Metadata();
  EXPECT_TRUE(actual_metadata.has_value());
  EXPECT_THAT(*actual_metadata, IsProtoEqual(expected_metadata));

  // Verify the returned values are correct.
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(10)));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value("user10")));

  // At end of stream, we get an 'ok' response with no value.
  auto eos = (*reader)->NextValue();
  EXPECT_STATUS_OK(eos);
  EXPECT_FALSE(eos->has_value());

  // Verify the returned stats are correct.
  spanner_proto::ResultSetStats expected_stats;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
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
      )pb",
      &expected_stats));
  auto actual_stats = (*reader)->Stats();
  EXPECT_TRUE(actual_stats.has_value());
  EXPECT_THAT(*actual_stats, IsProtoEqual(expected_stats));
}

/**
 * @test Verify the functionality of the PartialResultSetReader when the gRPC
 * reader returns data across multiple Read() calls.
 */
TEST(PartialResultSetReaderTest, MultipleResponses) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 5> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "10" }
        values: { string_value: "user10" }
      )pb",
      &response[1]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "22" }
        values: { string_value: "user22" }
      )pb",
      &response[2]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "99" }
      )pb",
      &response[3]));
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[4]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[2]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[3]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[4]), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned values are correct.
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(10)));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value("user10")));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(22)));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value("user22")));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(99)));
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value("99user99")));

  // At end of stream, we get an 'ok' response with no value.
  auto eos = (*reader)->NextValue();
  EXPECT_STATUS_OK(eos);
  EXPECT_FALSE(eos->has_value());
}

/**
 * @test Verify the behavior when a response with no values is received.
 */
TEST(PartialResultSetReaderTest, ResponseWithNoValues) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 3> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(TextFormat::ParseFromString("", &response[1]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "22" }
      )pb",
      &response[2]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[2]), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned value is correct.
  EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(22)));

  // At end of stream, we get an 'ok' response with no value.
  auto eos = (*reader)->NextValue();
  EXPECT_STATUS_OK(eos);
  EXPECT_FALSE(eos->has_value());
}

/**
 * @Test Verify reassembling chunked values works correctly, including a mixture
 * of chunked and unchunked values.
 */
TEST(PartialResultSetReaderTest, ChunkedStringValueWellFormed) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 5> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  // Note this is part of a value that spans 3 responses.
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "second_chunk" }
        chunked_value: true
      )pb",
      &response[1]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "third_chunk" }
        values: { string_value: "second group first_chunk " }
        chunked_value: true
      )pb",
      &response[2]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "second group second_chunk" }
        values: { string_value: "also not_chunked" }
      )pb",
      &response[3]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "still not_chunked" }
      )pb",
      &response[4]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[2]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[3]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[4]), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Verify the returned values are correct.
  for (const auto& value :
       {"not_chunked", "first_chunksecond_chunkthird_chunk",
        "second group first_chunk second group second_chunk",
        "also not_chunked", "still not_chunked"}) {
    EXPECT_THAT((*reader)->NextValue(), IsValidAndEquals(Value(value)));
  }

  // At end of stream, we get an 'ok' response with no value.
  auto eos = (*reader)->NextValue();
  EXPECT_STATUS_OK(eos);
  EXPECT_FALSE(eos->has_value());
}

/**
 * @test Verify the behavior when `chunked_value` is set but there are no
 * values in the response.
 */
TEST(PartialResultSetReaderTest, ChunkedValueSetNoValue) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 2> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(
      TextFormat::ParseFromString(R"pb(chunked_value: true)pb", &response[1]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next value should fail.
  auto value = (*reader)->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kInternal);
  EXPECT_EQ(
      value.status().message(),
      "PartialResultSet had chunked_value set true but contained no values");
}

/**
 * @test Verify the behavior when a response with no values follows one
 * with `chunked_value` set.
 */
TEST(PartialResultSetReaderTest, ChunkedValueSetNoFollowingValue) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 2> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb()pb", &response[1]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next value should fail.
  auto value = (*reader)->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kInternal);
  EXPECT_EQ(value.status().message(),
            "PartialResultSet contained no values to merge with prior "
            "chunked_value");
}

/**
 * @test Verify the behavior when `chunked_value` is set in the final response.
 */
TEST(PartialResultSetReaderTest, ChunkedValueSetAtEndOfStream) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 2> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { string_value: "incomplete" }
        chunked_value: true
      )pb",
      &response[1]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next value should fail.
  auto value = (*reader)->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kInternal);
  EXPECT_EQ(value.status().message(),
            "incomplete chunked_value at end of stream");
}

/**
 * @test Verify the behavior when attempting to merge a value that can't be
 * chunked (float64/number).
 */
TEST(PartialResultSetReaderTest, ChunkedValueMergeFailure) {
  auto grpc_reader = make_unique<MockGrpcReader>();
  std::array<spanner_proto::PartialResultSet, 3> response;
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      &response[0]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { number_value: 86 }
        chunked_value: true
      )pb",
      &response[1]));
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        values: { number_value: 99 }
      )pb",
      &response[2]));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response[0]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[1]), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(response[2]), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));

  auto context = make_unique<grpc::ClientContext>();
  auto reader = PartialResultSetReader::Create(std::move(context),
                                               std::move(grpc_reader));
  EXPECT_STATUS_OK(reader.status());

  // Trying to read the next value should fail.
  auto value = (*reader)->NextValue();
  EXPECT_EQ(value.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(value.status().message(), "invalid type");
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
