// Copyright 2022 Google LLC
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

#include "google/cloud/internal/rest_log_wrapper.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/tracing_options.h"
#include <google/iam/admin/v1/iam.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;

/// @test the overload for functions returning Status
TEST(RestLogWrapper, StatusValue) {
  auto mock = [](rest_internal::RestContext&,
                 google::iam::admin::v1::DeleteServiceAccountRequest const&) {
    return Status{};
  };

  testing_util::ScopedLog log;
  rest_internal::RestContext context;
  google::iam::admin::v1::DeleteServiceAccountRequest request;
  auto const r =
      RestLogWrapper(mock, context, request, "in-test", TracingOptions{});
  EXPECT_TRUE(r.ok());
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
}

/// @test the overload for functions returning erroneous Status
TEST(RestLogWrapper, StatusValueError) {
  auto status = Status(StatusCode::kPermissionDenied, "uh-oh");
  auto mock = [&status](
                  rest_internal::RestContext&,
                  google::iam::admin::v1::DeleteServiceAccountRequest const&) {
    return status;
  };

  testing_util::ScopedLog log;
  rest_internal::RestContext context;
  google::iam::admin::v1::DeleteServiceAccountRequest request;
  auto const r = RestLogWrapper(mock, context, request, "in-test", {});
  EXPECT_EQ(r.code(), status.code());
  EXPECT_EQ(r.message(), status.message());
  std::ostringstream os;
  os << status.message();
  auto status_as_string = std::move(os).str();
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="),
                             HasSubstr(status_as_string))));
}

/// @test the overload for functions returning StatusOr
TEST(RestLogWrapper, StatusOrValue) {
  auto mock = [](rest_internal::RestContext&,
                 google::iam::admin::v1::ListServiceAccountsRequest const&)
      -> StatusOr<google::iam::admin::v1::ListServiceAccountsResponse> {
    return google::iam::admin::v1::ListServiceAccountsResponse{};
  };

  testing_util::ScopedLog log;
  rest_internal::RestContext context;
  google::iam::admin::v1::ListServiceAccountsRequest request;
  auto const r =
      RestLogWrapper(mock, context, request, "in-test", TracingOptions{});
  EXPECT_TRUE(r.ok());
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
}

/// @test the overload for functions returning erroneous StatusOr
TEST(RestLogWrapper, StatusOrValueError) {
  auto status = Status(StatusCode::kPermissionDenied, "uh-oh");
  auto mock = [&status](
                  rest_internal::RestContext&,
                  google::iam::admin::v1::ListServiceAccountsRequest const&) {
    return status;
  };

  testing_util::ScopedLog log;
  rest_internal::RestContext context;
  google::iam::admin::v1::ListServiceAccountsRequest request;
  auto const r = RestLogWrapper(mock, context, request, "in-test", {});
  EXPECT_EQ(r.code(), status.code());
  EXPECT_EQ(r.message(), status.message());
  std::ostringstream os;
  os << status.message();
  auto status_as_string = std::move(os).str();
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" << "))));
  EXPECT_THAT(log_lines,
              Contains(AllOf(HasSubstr("in-test("), HasSubstr(" >> status="),
                             HasSubstr(status_as_string))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
