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
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <google/spanner/v1/mutation.pb.h>
#include <gmock/gmock.h>

namespace btproto = ::google::bigtable::v2;

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

TEST(LogWrapper, StatusDetails) {
  google::rpc::ResourceInfo resource_info;
  resource_info.set_resource_type(
      "type.googleapis.com/google.cloud.service.v1.Resource");
  resource_info.set_resource_name("projects/project/resources/resource");
  resource_info.set_description("Resource does not exist.");

  google::rpc::Status proto;
  proto.set_code(grpc::StatusCode::NOT_FOUND);
  proto.set_message("Resource not found");
  proto.add_details()->PackFrom(resource_info);

  TracingOptions tracing_options;
  auto s = DebugString(MakeStatusFromRpcError(proto), tracing_options);
  EXPECT_THAT(s, HasSubstr("NOT_FOUND: Resource not found"));
  EXPECT_THAT(s, HasSubstr(" + google.rpc.ResourceInfo {"));
  EXPECT_THAT(s, HasSubstr(resource_info.resource_type()));
  EXPECT_THAT(s, HasSubstr(resource_info.resource_name()));
  EXPECT_THAT(s, HasSubstr(resource_info.description()));
}

/// @test the overload for functions returning FutureStatusOr
TEST(LogWrapper, FutureStatusOrValue) {
  auto mock = [](google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  testing_util::ScopedLog log;

  LogWrapper(mock, MakeMutation(), "in-test", {});

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> response="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning FutureStatusOr
TEST(LogWrapper, FutureStatusOrError) {
  auto mock = [](google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  testing_util::ScopedLog log;

  LogWrapper(mock, MakeMutation(), "in-test", {});

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

/// @test the overload for functions returning FutureStatusOr and using
/// CompletionQueue as input
TEST(LogWrapper, FutureStatusOrValueWithContextAndCQ) {
  auto mock = [](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext>,
                 google::spanner::v1::Mutation m) {
    return make_ready_future(make_status_or(std::move(m)));
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
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
  auto mock = [](google::cloud::CompletionQueue&,
                 std::unique_ptr<grpc::ClientContext>,
                 google::spanner::v1::Mutation const&) {
    return make_ready_future(StatusOr<google::spanner::v1::Mutation>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
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
  auto mock = [&](google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::spanner::v1::Mutation const&) {
    return make_ready_future(status);
  };

  testing_util::ScopedLog log;

  CompletionQueue cq;
  std::unique_ptr<grpc::ClientContext> context;
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  std::ostringstream os;
  os << status;
  auto status_as_string = std::move(os).str();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> response=" + status_as_string))));
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
  auto context = absl::make_unique<TestContext>();
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
  auto context = absl::make_unique<TestContext>();
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
  auto context = absl::make_unique<TestContext>();
  LogWrapper(mock, cq, std::move(context), MakeMutation(), "in-test", {});

  std::ostringstream os;
  os << status;
  auto status_as_string = std::move(os).str();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("),
                             HasSubstr(" >> response=" + status_as_string))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

/// @test the overload for functions returning Status
TEST(LogWrapper, StatusValue) {
  auto mock = [](grpc::ClientContext*, btproto::MutateRowRequest const&,
                 btproto::MutateRowResponse*) { return grpc::Status(); };

  testing_util::ScopedLog log;
  grpc::ClientContext context;
  btproto::MutateRowRequest request;
  btproto::MutateRowResponse response;
  auto const r = LogWrapper(mock, &context, request, &response, "in-test", {});
  EXPECT_TRUE(r.ok());

  auto const log_lines = log.ExtractLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
}

/// @test the overload for functions returning erroneous Status
TEST(LogWrapper, StatusValueError) {
  auto status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh");
  auto mock = [&status](grpc::ClientContext*, btproto::MutateRowRequest const&,
                        btproto::MutateRowResponse*) { return status; };

  testing_util::ScopedLog log;
  grpc::ClientContext context;
  btproto::MutateRowRequest request;
  btproto::MutateRowResponse response;
  auto const r = LogWrapper(mock, &context, request, &response, "in-test", {});
  EXPECT_EQ(r.error_code(), status.error_code());
  EXPECT_EQ(r.error_message(), status.error_message());

  std::ostringstream os;
  os << status.error_message();
  auto status_as_string = std::move(os).str();

  auto const log_lines = log.ExtractLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="),
                             HasSubstr(status_as_string))));
}

/// @test the overload for functions returning ClientReaderInterface
TEST(LogWrapper, UniquePointerClientReaderInterface) {
  auto mock = [](grpc::ClientContext*, btproto::ReadRowsRequest const&) {
    return std::unique_ptr<
        grpc::ClientReaderInterface<btproto::ReadRowsResponse>>{};
  };

  testing_util::ScopedLog log;
  grpc::ClientContext context;
  btproto::ReadRowsRequest request;
  LogWrapper(mock, &context, request, "in-test", {});

  auto const log_lines = log.ExtractLines();

  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
