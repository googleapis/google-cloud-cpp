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

#include "google/cloud/spanner/internal/partial_result_set_resume.h"
#include "google/cloud/spanner/testing/mock_partial_result_set_reader.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

using ::google::cloud::internal::Idempotency;
using ::google::cloud::spanner_testing::MockPartialResultSetReader;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Return;

using ReadReturn = absl::optional<spanner_proto::PartialResultSet>;

struct MockFactory {
  MOCK_METHOD(std::unique_ptr<PartialResultSetReader>, MakeReader,
              (std::string const& token));
};

std::unique_ptr<PartialResultSetReader> MakeTestResume(
    PartialResultSetReaderFactory factory, Idempotency idempotency) {
  return absl::make_unique<PartialResultSetResume>(
      std::move(factory), idempotency,
      spanner::LimitedErrorCountRetryPolicy(/*maximum_failures=*/2).clone(),
      spanner::ExponentialBackoffPolicy(
          /*initial_delay=*/std::chrono::microseconds(1),
          /*maximum_delay=*/std::chrono::microseconds(1),
          /*scaling=*/2.0)
          .clone());
}

TEST(PartialResultSetResume, Success) {
  spanner_proto::PartialResultSet response;
  auto constexpr kText =
      R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    resume_token: "test-token-0"
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
      )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read())
            .WillOnce([&response] { return ReadReturn(response); })
            .WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read();
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(*v, IsProtoEqual(response));
  v = reader->Read();
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, SuccessWithRestart) {
  auto constexpr kText0 = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    resume_token: "test-token-0"
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
  )pb";
  spanner_proto::PartialResultSet r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &r0));

  auto constexpr kText1 = R"pb(
    resume_token: "test-token-1"
    values: { string_value: "value-3" }
    values: { string_value: "value-4" }
  )pb";
  spanner_proto::PartialResultSet r1;
  ASSERT_TRUE(TextFormat::ParseFromString(kText1, &r1));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r0](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read())
            .WillOnce([&r0] { return ReadReturn(r0); })
            .WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again-0")));
        return mock;
      })
      .WillOnce([&r1](std::string const& token) {
        EXPECT_EQ("test-token-0", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read())
            .WillOnce([&r1] { return ReadReturn(r1); })
            .WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again-1")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("test-token-1", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read()).WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read();
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(*v, IsProtoEqual(r0));
  v = reader->Read();
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(*v, IsProtoEqual(r1));
  v = reader->Read();
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, PermanentError) {
  auto constexpr kText =
      R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    resume_token: "test-token-0"
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
      )pb";
  spanner_proto::PartialResultSet r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &r0));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r0](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read())
            .WillOnce([&r0] { return ReadReturn(r0); })
            .WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again-0")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("test-token-0", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read()).WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh-1")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read();
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(*v, IsProtoEqual(r0));
  v = reader->Read();
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh-1")));
}

TEST(PartialResultSetResume, TransientNonIdempotent) {
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    resume_token: "test-token-0"
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
  )pb";
  spanner_proto::PartialResultSet r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &r0));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r0](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read())
            .WillOnce([&r0] { return ReadReturn(r0); })
            .WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again-0")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kNonIdempotent);
  auto v = reader->Read();
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(*v, IsProtoEqual(r0));
  v = reader->Read();
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again-0")));
}

TEST(PartialResultSetResume, TooManyTransients) {
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .Times(AtLeast(2))
      .WillRepeatedly([](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read()).WillOnce(Return(ReadReturn{}));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again-N")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read();
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again-N")));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
