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
#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/tracing_options.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include <gmock/gmock.h>
#include <memory>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::NotNull;

using Request = google::protobuf::Duration;
using Response = google::protobuf::Timestamp;

struct TestOption {
  using Type = std::string;
};
struct TestContext {};

template <typename T>
class LogWrapperTest : public ::testing::Test {
 public:
  using TestTypes = T;
};

using TestVariations = ::testing::Types<
    std::tuple<Status, std::unique_ptr<TestContext>>,                     //
    std::tuple<StatusOr<Response>, std::unique_ptr<TestContext>>,         //
    std::tuple<Status, std::shared_ptr<grpc::ClientContext>>,             //
    std::tuple<StatusOr<Response>, std::shared_ptr<grpc::ClientContext>>  //
    >;

TYPED_TEST_SUITE(LogWrapperTest, TestVariations);

// These helper functions make it easier to write the tests.
Request MakeRequest() {
  Request r;
  r.set_seconds(42);
  return r;
}

Response MakeResponse() {
  Response r;
  r.set_seconds(123);
  r.set_nanos(456);
  return r;
}

Status SuccessValue(Status const&) { return Status{}; }
StatusOr<Response> SuccessValue(StatusOr<Response> const&) {
  return MakeResponse();
}

Status ErrorValue(Status const&) {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}
StatusOr<Response> ErrorValue(StatusOr<Response> const&) {
  return ErrorValue(Status{});
}

Matcher<Status> IsSuccess(Status const&) { return IsOk(); }
Matcher<StatusOr<Response>> IsSuccess(StatusOr<Response> const&) {
  return IsOkAndHolds(IsProtoEqual(MakeResponse()));
}
Matcher<Status> IsError(Status const&) {
  auto expected = ErrorValue(Status{});
  return StatusIs(expected.code(), expected.message());
}
Matcher<StatusOr<Response>> IsError(StatusOr<Response> const&) {
  auto expected = ErrorValue(Status{});
  return StatusIs(expected.code(), expected.message());
}

std::unique_ptr<TestContext> MakeContext(std::unique_ptr<TestContext> const&) {
  return std::make_unique<TestContext>();
}
std::shared_ptr<grpc::ClientContext> MakeContext(
    std::shared_ptr<grpc::ClientContext> const&) {
  return std::make_shared<grpc::ClientContext>();
}
std::string SuccessMarker(Status const&) { return "status=OK"; }
std::string SuccessMarker(StatusOr<Response> const&) {
  return "response=" + DebugString(MakeResponse(), TracingOptions{});
}

// A non-parametric test is simpler in this case.
TEST(LogWrapperUniquePtr, Success) {
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto functor = [](std::shared_ptr<grpc::ClientContext>, Request const&) {
    return std::make_unique<Response>(MakeResponse());
  };

  testing_util::ScopedLog log;

  auto context = std::make_shared<grpc::ClientContext>();
  auto actual =
      LogWrapper(functor, context, MakeRequest(), "in-test", TracingOptions{});
  ASSERT_THAT(actual, NotNull());
  EXPECT_THAT(*actual, IsProtoEqual(MakeResponse()));

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> not null"))));
}

// A non-parametric test is simpler in this case.
TEST(LogWrapperUniquePtr, Error) {
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto functor = [](std::shared_ptr<grpc::ClientContext>, Request const&) {
    return std::unique_ptr<Response>();
  };

  testing_util::ScopedLog log;

  auto context = std::make_shared<grpc::ClientContext>();
  auto actual =
      LogWrapper(functor, context, MakeRequest(), "in-test", TracingOptions{});
  ASSERT_THAT(actual, IsNull());

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> null"))));
}

TYPED_TEST(LogWrapperTest, BlockingSuccess) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextPtrType =
      std::tuple_element_t<1, typename TestFixture::TestTypes>;
  using ContextType = typename ContextPtrType::element_type;

  auto functor = [](ContextType&, Request const&) {
    return SuccessValue(ReturnType{});
  };

  testing_util::ScopedLog log;

  auto context = MakeContext(ContextPtrType{});
  auto actual =
      LogWrapper(functor, *context, MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual, IsSuccess(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  auto const expected_response = SuccessMarker(ReturnType{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> "),
                             HasSubstr(expected_response))));
}

TYPED_TEST(LogWrapperTest, BlockingError) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextPtrType =
      std::tuple_element_t<1, typename TestFixture::TestTypes>;
  using ContextType = typename ContextPtrType::element_type;

  auto functor = [](ContextType&, Request const&) {
    return ErrorValue(ReturnType{});
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextPtrType{});
  auto actual =
      LogWrapper(functor, *context, MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual, IsError(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
}

TYPED_TEST(LogWrapperTest, AsyncSuccess) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextType = std::tuple_element_t<1, typename TestFixture::TestTypes>;

  auto functor = [](google::cloud::CompletionQueue&, ContextType,
                    Request const&) {
    return make_ready_future(SuccessValue(ReturnType{}));
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextType{});
  auto actual = LogWrapper(functor, cq, std::move(context), MakeRequest(),
                           "in-test", TracingOptions{});
  EXPECT_THAT(actual.get(), IsSuccess(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  auto const expected_response = SuccessMarker(ReturnType{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> "),
                             HasSubstr(expected_response))));
}

TYPED_TEST(LogWrapperTest, AsyncError) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextType = std::tuple_element_t<1, typename TestFixture::TestTypes>;

  auto functor = [](google::cloud::CompletionQueue&, ContextType,
                    Request const&) {
    return make_ready_future(ErrorValue(ReturnType{}));
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextType{});
  auto actual = LogWrapper(functor, cq, std::move(context), MakeRequest(),
                           "in-test", TracingOptions{});
  EXPECT_THAT(actual.get(), IsError(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

TYPED_TEST(LogWrapperTest, BlockingSuccessExplicitOptions) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextPtrType =
      std::tuple_element_t<1, typename TestFixture::TestTypes>;
  using ContextType = typename ContextPtrType::element_type;

  auto functor = [](ContextType&, Options const& opts, Request const&) {
    EXPECT_EQ(opts.get<TestOption>(), "test-option");
    return SuccessValue(ReturnType{});
  };

  testing_util::ScopedLog log;

  auto context = MakeContext(ContextPtrType{});
  auto actual =
      LogWrapper(functor, *context, Options{}.set<TestOption>("test-option"),
                 MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual, IsSuccess(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  auto const expected_response = SuccessMarker(ReturnType{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> "),
                             HasSubstr(expected_response))));
}

TYPED_TEST(LogWrapperTest, BlockingErrorExplicitOptions) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextPtrType =
      std::tuple_element_t<1, typename TestFixture::TestTypes>;
  using ContextType = typename ContextPtrType::element_type;

  auto functor = [](ContextType&, Options const& opts, Request const&) {
    EXPECT_EQ(opts.get<TestOption>(), "test-option");
    return ErrorValue(ReturnType{});
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextPtrType{});
  auto actual =
      LogWrapper(functor, *context, Options{}.set<TestOption>("test-option"),
                 MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual, IsError(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
}

TYPED_TEST(LogWrapperTest, AsyncSuccessExplicitOptions) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextType = std::tuple_element_t<1, typename TestFixture::TestTypes>;

  auto functor = [](google::cloud::CompletionQueue&, ContextType,
                    Options const& opts, Request const&) {
    EXPECT_EQ(opts.get<TestOption>(), "test-option");
    return make_ready_future(SuccessValue(ReturnType{}));
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextType{});
  auto actual = LogWrapper(functor, cq, std::move(context),
                           Options{}.set<TestOption>("test-option"),
                           MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual.get(), IsSuccess(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  auto const expected_request = DebugString(MakeRequest(), TracingOptions{});
  auto const expected_response = SuccessMarker(ReturnType{});
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "),
                             HasSubstr(expected_request))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> "),
                             HasSubstr(expected_response))));
}

TYPED_TEST(LogWrapperTest, AsyncErrorExplicitOptions) {
  using ReturnType = std::tuple_element_t<0, typename TestFixture::TestTypes>;
  using ContextType = std::tuple_element_t<1, typename TestFixture::TestTypes>;

  auto functor = [](google::cloud::CompletionQueue&, ContextType,
                    Options const& opts, Request const&) {
    EXPECT_EQ(opts.get<TestOption>(), "test-option");
    return make_ready_future(ErrorValue(ReturnType{}));
  };

  testing_util::ScopedLog log;

  google::cloud::CompletionQueue cq;
  auto context = MakeContext(ContextType{});
  auto actual = LogWrapper(functor, cq, std::move(context),
                           Options{}.set<TestOption>("test-option"),
                           MakeRequest(), "in-test", TracingOptions{});
  EXPECT_THAT(actual.get(), IsError(ReturnType{}));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="))));
  EXPECT_THAT(log_lines, Contains(AllOf(HasSubstr("in-test("),
                                        HasSubstr(" >> future_status="))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
