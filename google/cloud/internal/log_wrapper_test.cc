// Copyright 2020 Google LLC
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

#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/tracing_options.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <google/spanner/v1/mutation.pb.h>
#include <gmock/gmock.h>

namespace btproto = google::bigtable::v2;

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
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

TEST(LogWrapper, DefaultOptions) {
  TracingOptions tracing_options;
  // clang-format off
  std::string const text =
      R"pb(insert { )pb"
      R"pb(table: "Singers" )pb"
      R"pb(columns: "SingerId" )pb"
      R"pb(columns: "FirstName" )pb"
      R"pb(columns: "LastName" )pb"
      R"pb(values { values { string_value: "1" } )pb"
      R"pb(values { string_value: "test-fname-1" } )pb"
      R"pb(values { string_value: "test-lname-1" } } )pb"
      R"pb(values { values { string_value: "2" } )pb"
      R"pb(values { string_value: "test-fname-2" } )pb"
      R"pb(values { string_value: "test-lname-2" } )pb"
      R"pb(} )pb"
      R"pb(} )pb";
  // clang-format on
  EXPECT_EQ(text, internal::DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapper, MultiLine) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("single_line_mode=off");
  // clang-format off
  std::string const text = R"pb(insert {
  table: "Singers"
  columns: "SingerId"
  columns: "FirstName"
  columns: "LastName"
  values {
    values {
      string_value: "1"
    }
    values {
      string_value: "test-fname-1"
    }
    values {
      string_value: "test-lname-1"
    }
  }
  values {
    values {
      string_value: "2"
    }
    values {
      string_value: "test-fname-2"
    }
    values {
      string_value: "test-lname-2"
    }
  }
}
)pb";
  // clang-format on
  EXPECT_EQ(text, internal::DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapper, Truncate) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  // clang-format off
  std::string const text =R"pb(insert { )pb"
            R"pb(table: "Singers" )pb"
            R"pb(columns: "SingerId" )pb"
            R"pb(columns: "FirstNam...<truncated>..." )pb"
            R"pb(columns: "LastName" )pb"
            R"pb(values { values { string_value: "1" } )pb"
            R"pb(values { string_value: "test-fna...<truncated>..." } )pb"
            R"pb(values { string_value: "test-lna...<truncated>..." } } )pb"
            R"pb(values { values { string_value: "2" } )pb"
            R"pb(values { string_value: "test-fna...<truncated>..." } )pb"
            R"pb(values { string_value: "test-lna...<truncated>..." } )pb"
            R"pb(} )pb"
            R"pb(} )pb";
  // clang-format on
  EXPECT_EQ(text, internal::DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapper, FutureStatus) {
  struct Case {
    std::future_status actual;
    std::string expected;
  } cases[] = {
      {std::future_status::deferred, "deferred"},
      {std::future_status::timeout, "timeout"},
      {std::future_status::ready, "ready"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(c.expected, DebugFutureStatus(c.actual));
  }
}

/// @test the overload for functions returning FutureStatusOr
TEST(LogWrapper, FutureStatusOrValue) {
  auto mock = [](google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  LogWrapper(mock, MakeMutation(), "in-test", {});

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> response="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning FutureStatusOr
TEST(LogWrapper, FutureStatusOrError) {
  auto mock = [](google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  LogWrapper(mock, MakeMutation(), "in-test", {});

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr("uh-oh"))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrValueWithContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext>,
                 google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> response="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrErrorWithContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext>,
                 google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr("uh-oh"))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning FutureStatus and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusWithContextAndCQ) {
  auto const status = Status(StatusCode::kPermissionDenied, "uh-oh");
  auto mock = [&](google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::spanner::v1::Mutation const&) {
    return make_ready_future(status);
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  std::ostringstream os;
  os << status;
  auto status_as_string = std::move(os).str();

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> response=" + status_as_string))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning Status
TEST(LogWrapper, StatusValue) {
  auto mock = [](grpc::ClientContext*, btproto::MutateRowRequest const&,
                 btproto::MutateRowResponse*) { return grpc::Status(); };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);
  grpc::ClientContext context;
  btproto::MutateRowRequest request;
  btproto::MutateRowResponse response;
  auto const r = LogWrapper(mock, &context, request, &response, "in-test", {});
  EXPECT_TRUE(r.ok());

  auto const log_lines = backend->ClearLogLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning erroneous Status
TEST(LogWrapper, StatusValueError) {
  auto status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh");
  auto mock = [&status](grpc::ClientContext*, btproto::MutateRowRequest const&,
                        btproto::MutateRowResponse*) { return status; };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);
  grpc::ClientContext context;
  btproto::MutateRowRequest request;
  btproto::MutateRowResponse response;
  auto const r = LogWrapper(mock, &context, request, &response, "in-test", {});
  EXPECT_EQ(r.error_code(), status.error_code());
  EXPECT_EQ(r.error_message(), status.error_message());

  std::ostringstream os;
  os << status.error_message();
  auto status_as_string = std::move(os).str();

  auto const log_lines = backend->ClearLogLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> status=" + status_as_string))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test the overload for functions returning ClientReaderInterface
TEST(LogWrapper, UniquePointerClientReaderInterface) {
  auto mock = [](grpc::ClientContext*, btproto::ReadRowsRequest const&) {
    return std::unique_ptr<
        grpc::ClientReaderInterface<btproto::ReadRowsResponse>>{};
  };

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);
  grpc::ClientContext context;
  btproto::ReadRowsRequest request;
  LogWrapper(mock, &context, request, "in-test", {});

  auto const log_lines = backend->ClearLogLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
