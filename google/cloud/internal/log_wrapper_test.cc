// Copyright 2020 Google LLC
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

#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/tracing_options.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/spanner/v1/mutation.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;

google::spanner::v1::Mutation MakeMutation() {
  auto constexpr kText = R"pb(
    insert {
      table: "Singers"
      columns: "SingerId"
      columns: "FirstName"
      columns: "LastName"
      values {
        values { string_value: "1" }
        values { string_value: "test-fname-1" }
        values { string_value: "test-lname-1" }
      }
      values {
        values { string_value: "2" }
        values { string_value: "test-fname-2" }
        values { string_value: "test-lname-2" }
      }
    }
  )pb";
  google::spanner::v1::Mutation mutation;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &mutation));
  return mutation;
}

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrValueWithContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&, auto,
                 google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::shared_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> response="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrErrorWithContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&, auto,
                 google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::shared_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr("uh-oh"))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning FutureStatus and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusWithContextAndCQ) {
  auto const status = Status(StatusCode::kPermissionDenied, "uh-oh");
  auto mock = [&](google::cloud::CompletionQueue&, auto,
                  google::spanner::v1::Mutation const&) {
    return make_ready_future(status);
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::shared_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  std::ostringstream os;
  os << status;
  auto status_as_string = std::move(os).str();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> status=" + status_as_string))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

struct TestContext {};

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrValueWithTestContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&, std::unique_ptr<TestContext>,
                 google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  testing_util::ScopedLog log;
  CompletionQueue cq;
  auto context = std::make_unique<TestContext>();
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> response="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrErrorWithTestContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&, std::unique_ptr<TestContext>,
                 google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  testing_util::ScopedLog log;
  CompletionQueue cq;
  auto context = std::make_unique<TestContext>();
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr("uh-oh"))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning FutureStatus and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusWithTestContextAndCQ) {
  auto const status = Status(StatusCode::kPermissionDenied, "uh-oh");
  auto mock = [&](google::cloud::CompletionQueue&, std::unique_ptr<TestContext>,
                  google::spanner::v1::Mutation const&) {
    return make_ready_future(status);
  };

  testing_util::ScopedLog log;
  CompletionQueue cq;
  auto context = std::make_unique<TestContext>();
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  std::ostringstream os;
  os << status;
  auto status_as_string = std::move(os).str();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> status=" + status_as_string))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
