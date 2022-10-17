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

#include "google/cloud/internal/log_wrapper_helpers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/tracing_options.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/rpc/error_details.pb.h>
#include <google/rpc/status.pb.h>
#include <google/spanner/v1/mutation.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

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

TEST(LogWrapperHelpers, DefaultOptions) {
  TracingOptions tracing_options;
  // clang-format off
  std::string const text =
      R"pb(google.spanner.v1.Mutation { )pb"
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
      R"pb(} )pb"
      R"pb(})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapperHelpers, MultiLine) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("single_line_mode=off");
  // clang-format off
  std::string const text = R"pb(google.spanner.v1.Mutation {
  insert {
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
})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapperHelpers, Truncate) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  // clang-format off
  std::string const text =
            R"pb(google.spanner.v1.Mutation { )pb"
            R"pb(insert { )pb"
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
            R"pb(} )pb"
            R"pb(})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakeMutation(), tracing_options));
}

TEST(LogWrapperHelpers, TruncateString) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  struct Case {
    std::string actual;
    std::string expected;
  } cases[] = {
      {"1234567", "1234567"},
      {"12345678", "12345678"},
      {"123456789", "12345678...<truncated>..."},
      {"1234567890", "12345678...<truncated>..."},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(c.expected, DebugString(c.actual, tracing_options));
  }
}

TEST(LogWrapperHelpers, Duration) {
  google::protobuf::Duration duration;
  duration.set_seconds((11 * 60 + 22) * 60 + 33);
  duration.set_nanos(123456789);
  std::string const expected =
      R"(google.protobuf.Duration { "11h22m33.123456789s" })";
  EXPECT_EQ(expected, DebugString(duration, TracingOptions{}.SetOptions(
                                                "single_line_mode=on")));
}

TEST(LogWrapperHelpers, Timestamp) {
  google::protobuf::Timestamp timestamp;
  timestamp.set_seconds(1658470436);
  timestamp.set_nanos(123456789);
  std::string const expected = R"(google.protobuf.Timestamp {
  "2022-07-22T06:13:56.123456789Z"
})";
  EXPECT_EQ(expected, DebugString(timestamp, TracingOptions{}.SetOptions(
                                                 "single_line_mode=off")));
}

TEST(LogWrapperHelpers, FutureStatus) {
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
