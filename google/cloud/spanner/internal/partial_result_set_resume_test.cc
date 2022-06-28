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

#include "google/cloud/spanner/internal/partial_result_set_resume.h"
#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/mock_partial_result_set_reader.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/options.h"
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::Idempotency;
using ::google::cloud::spanner_testing::MockPartialResultSetReader;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Return;

absl::optional<PartialResultSet> ReadReturn(
    google::spanner::v1::PartialResultSet response) {
  bool const resumption = false;  // only a PartialResultSetResume returns true
  return PartialResultSet{std::move(response), resumption};
}

absl::optional<PartialResultSet> ReadReturn() { return {}; }

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

StatusOr<std::unique_ptr<ResultSourceInterface>> CreatePartialResultSetSource(
    std::unique_ptr<PartialResultSetReader> reader, Options opts = {}) {
  internal::OptionsSpan span(
      internal::MergeOptions(std::move(opts), internal::CurrentOptions()));
  return PartialResultSetSource::Create(std::move(reader));
}

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<Row> contains the given Row") {
  return arg && *arg == expected;
}

TEST(PartialResultSetResume, Success) {
  google::spanner::v1::PartialResultSet response;
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
    resume_token: "resume-after-2"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read("");
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(v->result, IsProtoEqual(response));
  v = reader->Read("resume-after-2");
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, SuccessWithRestart) {
  auto constexpr kText12 = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
    resume_token: "resume-after-2"
  )pb";
  google::spanner::v1::PartialResultSet r12;
  ASSERT_TRUE(TextFormat::ParseFromString(kText12, &r12));

  auto constexpr kText34 = R"pb(
    values: { string_value: "value-3" }
    values: { string_value: "value-4" }
    resume_token: "resume-after-4"
  )pb";
  google::spanner::v1::PartialResultSet r34;
  ASSERT_TRUE(TextFormat::ParseFromString(kText34, &r34));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&r12] { return ReadReturn(r12); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again 1")));
        return mock;
      })
      .WillOnce([&r34](std::string const& token) {
        EXPECT_EQ("resume-after-2", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&r34] { return ReadReturn(r34); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again 2")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("resume-after-4", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_)).WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read("");
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(v->result, IsProtoEqual(r12));
  v = reader->Read("resume-after-2");
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(v->result, IsProtoEqual(r34));
  v = reader->Read("resume-after-4");
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_STATUS_OK(status);
}

TEST(PartialResultSetResume, PermanentError) {
  auto constexpr kText12 = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
    resume_token: "resume-after-2"
  )pb";
  google::spanner::v1::PartialResultSet r12;
  ASSERT_TRUE(TextFormat::ParseFromString(kText12, &r12));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&r12] { return ReadReturn(r12); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      })
      .WillOnce([](std::string const& token) {
        EXPECT_EQ("resume-after-2", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_)).WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read("");
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(v->result, IsProtoEqual(r12));
  v = reader->Read("resume-after-2");
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PartialResultSetResume, TransientNonIdempotent) {
  auto constexpr kText12 = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "TestColumn",
          type: { code: STRING }
        }
      }
    }
    values: { string_value: "value-1" }
    values: { string_value: "value-2" }
    resume_token: "resume-after-2"
  )pb";
  google::spanner::v1::PartialResultSet r12;
  ASSERT_TRUE(TextFormat::ParseFromString(kText12, &r12));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&r12](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&r12] { return ReadReturn(r12); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kNonIdempotent);
  auto v = reader->Read("");
  ASSERT_TRUE(v.has_value());
  EXPECT_THAT(v->result, IsProtoEqual(r12));
  v = reader->Read("resume-after-2");
  ASSERT_FALSE(v.has_value());
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
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_)).WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto v = reader->Read("");
  ASSERT_FALSE(v.has_value());
  auto status = reader->Finish();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("Try again")));
}

TEST(PartialResultSetResume, ResumptionStart) {
  std::array<char const*, 3> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "TestColumn",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "value-1" }
        values: { string_value: "value-2" }
      )pb",
      R"pb(
        values: { string_value: "value-3" }
        values: { string_value: "value-4" }
      )pb",
      R"pb(
        values: { string_value: "value-5" }
        values: { string_value: "value-6" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[0]); })
            .WillOnce([&response] { return ReadReturn(response[1]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      })
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[0]); })
            .WillOnce([&response] { return ReadReturn(response[1]); })
            .WillOnce([&response] { return ReadReturn(response[2]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned rows are correct, despite the resumption from the
  // beginning of the stream after the transient error.
  for (auto const* s : {"value-1", "value-2", "value-3",  //
                        "value-4", "value-5", "value-6"}) {
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                          {"TestColumn", spanner::Value(s)},
                                      })));
  }
  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

TEST(PartialResultSetResume, ResumptionMidway) {
  std::array<char const*, 3> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "TestColumn",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "value-1" }
        values: { string_value: "value-2" }
      )pb",
      R"pb(
        values: { string_value: "value-3" }
        values: { string_value: "value-4" }
        resume_token: "resume-after-4"
      )pb",
      R"pb(
        values: { string_value: "value-5" }
        values: { string_value: "value-6" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[0]); })
            .WillOnce([&response] { return ReadReturn(response[1]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      })
      .WillOnce([&response](std::string const& token) {
        EXPECT_EQ("resume-after-4", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[2]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  auto reader = CreatePartialResultSetSource(std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned rows are correct, despite the resumption from a
  // midway point in the stream after the transient error.
  for (auto const* s : {"value-1", "value-2", "value-3",  //
                        "value-4", "value-5", "value-6"}) {
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                          {"TestColumn", spanner::Value(s)},
                                      })));
  }
  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

TEST(PartialResultSetResume, ResumptionAfterResync) {
  std::array<char const*, 3> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "TestColumn",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "value-1" }
        values: { string_value: "value-2" }
      )pb",
      R"pb(
        values: { string_value: "value-3" }
        values: { string_value: "value-4" }
        resume_token: "resume-after-4"
      )pb",
      R"pb(
        values: { string_value: "value-5" }
        values: { string_value: "value-6" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[0]); })
            .WillOnce([&response] { return ReadReturn(response[1]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      })
      .WillOnce([&response](std::string const& token) {
        EXPECT_EQ("resume-after-4", token);
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[2]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish()).WillOnce(Return(Status()));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  // Disable buffering of rows not covered by a resume token.
  auto reader = CreatePartialResultSetSource(
      std::move(grpc_reader),
      Options{}.set<spanner::StreamingResumabilityBufferSizeOption>(0));
  ASSERT_STATUS_OK(reader);

  // Verify the returned rows are correct, despite the resumption from a
  // midway point in the stream after the transient error. Even though we
  // became non-resumable after yielding "value-2", we received a resume
  // token covering up to "value-4" before the error occurred.
  for (auto const* s : {"value-1", "value-2", "value-3",  //
                        "value-4", "value-5", "value-6"}) {
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                          {"TestColumn", spanner::Value(s)},
                                      })));
  }
  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner::Row{}));
}

TEST(PartialResultSetResume, ResumptionBeforeResync) {
  std::array<char const*, 1> text{{
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "TestColumn",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "value-1" }
        values: { string_value: "value-2" }
      )pb",
  }};
  std::array<google::spanner::v1::PartialResultSet, text.size()> response;
  for (std::size_t i = 0; i != text.size(); ++i) {
    SCOPED_TRACE("Converting text to proto [" + std::to_string(i) + "]");
    ASSERT_TRUE(TextFormat::ParseFromString(text[i], &response[i]));
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, MakeReader)
      .WillOnce([&response](std::string const& token) {
        EXPECT_TRUE(token.empty());
        auto mock = absl::make_unique<MockPartialResultSetReader>();
        EXPECT_CALL(*mock, Read(_))
            .WillOnce([&response] { return ReadReturn(response[0]); })
            .WillOnce(Return(ReadReturn()));
        EXPECT_CALL(*mock, TryCancel()).Times(0);
        EXPECT_CALL(*mock, Finish())
            .WillOnce(Return(Status(StatusCode::kUnavailable, "Try again")));
        return mock;
      });

  auto factory = [&mock_factory](std::string const& token) {
    return mock_factory.MakeReader(token);
  };
  auto grpc_reader = MakeTestResume(factory, Idempotency::kIdempotent);
  // Disable buffering of rows not covered by a resume token.
  auto reader = CreatePartialResultSetSource(
      std::move(grpc_reader),
      Options{}.set<spanner::StreamingResumabilityBufferSizeOption>(0));
  ASSERT_STATUS_OK(reader);

  // Verify the first two rows are returned.
  for (auto const* s : {"value-1", "value-2"}) {
    EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(spanner_mocks::MakeRow({
                                          {"TestColumn", spanner::Value(s)},
                                      })));
  }

  // However, the stream is non-resumable when the transient error occurs
  // (because we've yielded rows not covered by a resume token), so the error
  // is returned to the user.
  EXPECT_THAT((*reader)->NextRow(),
              StatusIs(StatusCode::kUnavailable, "Try again"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
