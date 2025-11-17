// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/partial_result_set_resume.h"
#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/bigtable/retry_policy.h"
#include "google/cloud/bigtable/testing/mock_partial_result_set_reader.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/substitute.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::Idempotency;
using ::google::cloud::bigtable_testing::MockPartialResultSetReader;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Return;

struct MockFactory {
  MOCK_METHOD(std::unique_ptr<PartialResultSetReader>, MakeReader,
              (std::string const& token));
};

std::unique_ptr<PartialResultSetReader> MakeTestResume(
    PartialResultSetReaderFactory factory, Idempotency idempotency) {
  return std::make_unique<PartialResultSetResume>(
      std::move(factory), idempotency,
      bigtable::DataLimitedErrorCountRetryPolicy(/*maximum_failures=*/2)
          .clone(),
      google::cloud::ExponentialBackoffPolicy(
          /*initial_delay=*/std::chrono::microseconds(1),
          /*maximum_delay=*/std::chrono::microseconds(1),
          /*scaling=*/2.0)
          .clone());
}

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<QueryRow> contains the given QueryRow") {
  return arg && *arg == expected;
}

auto constexpr kResultMetadataText = R"pb(
  proto_schema {
    columns {
      name: "TestColumn"
      type { string_type {} }
    }
  }
)pb";

void MakeResponse(google::bigtable::v2::PartialResultSet& response,
                  std::vector<std::string> const& values,
                  absl::optional<std::string> resume_token, bool reset) {
  google::bigtable::v2::ProtoRows proto_rows;
  for (auto const& v : values) {
    google::bigtable::v2::Value value;
    value.set_string_value(v);
    google::bigtable::v2::Type type;
    *(*value.mutable_type()).mutable_string_type() =
        std::move(google::bigtable::v2::Type_String{});
    *proto_rows.add_values() = value;
  }
  auto text = absl::Substitute(
      R"pb(
        proto_rows_batch: {},
            $0 $1
        estimated_batch_size: 31,
        batch_checksum: 123456
      )pb",
      resume_token ? absl::Substitute(R"(resume_token: "$0")", *resume_token)
                   : "",
      reset ? "reset: true," : "");
  ASSERT_TRUE(TextFormat::ParseFromString(text, &response));
  std::string binary_batch_data = proto_rows.SerializeAsString();
  *(*response.mutable_proto_rows_batch()).mutable_batch_data() =
      binary_batch_data;
}

// Helper function for MockPartialResultSetReader::Read to return true and
// populate result
auto ReadAction(google::bigtable::v2::PartialResultSet& response_proto,
                bool resumption_val) {
  return [&response_proto, resumption_val](absl::optional<std::string> const&,
                                           UnownedPartialResultSet& result) {
    result.result = response_proto;
    result.resumption = resumption_val;
    return true;
  };
};

TEST(PartialResultSetResume, Success) {
  auto constexpr kProtoRowsText = R"pb(
    values { string_value: "value-1" }
    values { string_value: "value-2" }
  )pb";
  google::bigtable::v2::ProtoRows proto_rows;
  ASSERT_TRUE(TextFormat::ParseFromString(kProtoRowsText, &proto_rows));

  std::string binary_batch_data = proto_rows.SerializeAsString();
  std::string partial_result_set_text =
      absl::Substitute(R"pb(
                         proto_rows_batch: {
                           batch_data: "$0",
                         },
                         resume_token: "resume-after-2",
                         reset: true,
                         estimated_batch_size: 31,
                         batch_checksum: 123456
                       )pb",
                       binary_batch_data);
  google::bigtable::v2::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(partial_result_set_text, &response));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(response, false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  google::bigtable::v2::PartialResultSet raw_result;
  auto result = UnownedPartialResultSet::FromPartialResultSet(raw_result);
  ASSERT_TRUE(reader->Read("", result));
  EXPECT_THAT(result.result, IsProtoEqual(response));
  ASSERT_FALSE(reader->Read("resume-after-2", result));
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, SuccessWithRestart) {
  google::bigtable::v2::PartialResultSet r12;
  google::bigtable::v2::PartialResultSet r34;

  MakeResponse(r12, {"value-1", "value-2"}, "resume-after-2", true);
  MakeResponse(r34, {"value-3", "value-4"}, "resume-after-4", true);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(r12, false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again 1")));
        return mock;
      })
      .WillOnce([&r34](std::string const& token) {
        EXPECT_EQ("resume-after-2", token);
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(r34, false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again 2")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("resume-after-4", token);
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _)).WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  google::bigtable::v2::PartialResultSet raw_result;
  auto result = UnownedPartialResultSet::FromPartialResultSet(raw_result);
  ASSERT_TRUE(reader->Read("", result));
  EXPECT_THAT(raw_result, IsProtoEqual(r12));
  ASSERT_TRUE(reader->Read("resume-after-2", result));
  EXPECT_THAT(raw_result, IsProtoEqual(r34));
  ASSERT_FALSE(reader->Read("resume-after-4", result));
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, PermanentError) {
  google::bigtable::v2::PartialResultSet r12;
  MakeResponse(r12, {"value-1", "value-2"}, "resume-after-2", true);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(r12, false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("resume-after-2", token);
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _)).WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  google::bigtable::v2::PartialResultSet raw_result;
  auto result = UnownedPartialResultSet::FromPartialResultSet(raw_result);
  ASSERT_TRUE(reader->Read("", result));
  EXPECT_THAT(result.result, IsProtoEqual(r12));
  ASSERT_FALSE(reader->Read("resume-after-2", result));
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PartialResultSetResume, TransientNonIdempotent) {
  google::bigtable::v2::PartialResultSet r12;
  MakeResponse(r12, {"value-1", "value-2"}, "resume-after-2", true);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(r12, false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kNonIdempotent);
  google::bigtable::v2::PartialResultSet raw_result;
  auto result = UnownedPartialResultSet::FromPartialResultSet(raw_result);
  ASSERT_TRUE(reader->Read("", result));
  EXPECT_THAT(result.result, IsProtoEqual(r12));
  ASSERT_FALSE(reader->Read("resume-after-2", result));
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("Try again")));
}

TEST(PartialResultSetResume, TooManyTransients) {
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .Times(AtLeast(2))
      .WillRepeatedly([](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _)).WillOnce(Return(false));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  google::bigtable::v2::PartialResultSet raw_result;
  auto result = UnownedPartialResultSet::FromPartialResultSet(raw_result);
  ASSERT_FALSE(reader->Read("", result));
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("Try again")));
  EXPECT_THAT(status.error_info().metadata(),
              ::testing::Contains(::testing::Pair("gcloud-cpp.retry.reason",
                                                  "retry-policy-exhausted")));
}

TEST(PartialResultSetResume, ResumptionStart) {
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kResultMetadataText, &metadata));

  std::vector<google::bigtable::v2::PartialResultSet> response;
  google::bigtable::v2::PartialResultSet r12;
  google::bigtable::v2::PartialResultSet r34;
  google::bigtable::v2::PartialResultSet r56;
  MakeResponse(r12, {"value-1", "value-2"}, absl::nullopt, true);
  MakeResponse(r34, {"value-3", "value-4"}, absl::nullopt, false);
  MakeResponse(r56, {"value-5", "value-6"}, "end-of-stream", false);
  response.push_back(r12);
  response.push_back(r34);
  response.push_back(r56);

  MockFactory mock_factory;
  grpc::ClientContext context;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(response[0], false))
            .WillOnce(ReadAction(response[1], false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        EXPECT_CALL(*mock, context)
            .Times(1)
            .WillRepeatedly(
                [&]() -> grpc::ClientContext const& { return context; });
        return mock;
      })
      .WillOnce([&](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(response[0], false))
            .WillOnce(ReadAction(response[1], false))
            .WillOnce(ReadAction(response[2], false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        EXPECT_CALL(*mock, context)
            .Times(13)
            .WillRepeatedly(
                [&]() -> grpc::ClientContext const& { return context; });
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto reader = PartialResultSetSource::Create(
      metadata, std::make_shared<OperationContext>(), std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned rows are correct, despite the resumption from the
  // beginning of the stream after the transient error.
  for (auto const* s :
       {"value-1", "value-2", "value-3", "value-4", "value-5", "value-6"}) {
    auto row = (*reader)->NextRow();
    EXPECT_THAT(row, IsValidAndEquals(bigtable_mocks::MakeQueryRow(
                         {{"TestColumn", bigtable::Value(s)}})));
  }

  auto row = (*reader)->NextRow();
  EXPECT_THAT(row, IsValidAndEquals(bigtable::QueryRow{}));
}

TEST(PartialResultSetResume, ResumptionMidway) {
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kResultMetadataText, &metadata));

  std::vector<google::bigtable::v2::PartialResultSet> response;
  google::bigtable::v2::PartialResultSet r12;
  google::bigtable::v2::PartialResultSet r34;
  google::bigtable::v2::PartialResultSet r56;
  MakeResponse(r12, {"value-1", "value-2"}, absl::nullopt, true);
  MakeResponse(r34, {"value-3", "value-4"}, "resume-after-4", false);
  MakeResponse(r56, {"value-5", "value-6"}, "end-of-stream", false);
  response.push_back(r12);
  response.push_back(r34);
  response.push_back(r56);

  MockFactory mock_factory;
  grpc::ClientContext context;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(response[0], false))
            .WillOnce(ReadAction(response[1], false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        EXPECT_CALL(*mock, context)
            .Times(9)
            .WillRepeatedly(
                [&]() -> grpc::ClientContext const& { return context; });
        return mock;
      })
      .WillOnce([&](std::string const& token) {
        EXPECT_EQ("resume-after-4", token);
        auto mock = std::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_, _))
            .WillOnce(ReadAction(response[2], false))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        EXPECT_CALL(*mock, context)
            .Times(5)
            .WillRepeatedly(
                [&]() -> grpc::ClientContext const& { return context; });
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto reader = PartialResultSetSource::Create(
      metadata, std::make_shared<OperationContext>(), std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned rows are correct, despite the resumption from a
  // midway point in the stream after the transient error.
  for (auto const* s :
       {"value-1", "value-2", "value-3", "value-4", "value-5", "value-6"}) {
    auto row = (*reader)->NextRow();
    EXPECT_THAT(row, IsValidAndEquals(bigtable_mocks::MakeQueryRow(
                         {{"TestColumn", bigtable::Value(s)}})));
  }

  auto row = (*reader)->NextRow();
  EXPECT_THAT(row, IsValidAndEquals(bigtable::QueryRow{}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
