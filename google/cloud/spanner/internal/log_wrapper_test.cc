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

#include "google/cloud/spanner/internal/log_wrapper.h"
#include "google/cloud/spanner/tracing_options.h"
#include <google/protobuf/text_format.h>
#include <google/spanner/v1/mutation.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
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

// bool single_line_mode_ = true;
// bool use_short_repeated_primitives_ = true;
// std::int64_t truncate_string_field_longer_than_ = 128;

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

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
