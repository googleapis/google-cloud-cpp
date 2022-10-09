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

#include "google/cloud/storage/bucket_cors_entry.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(CorsEntry, IOStream) {
  auto const to_string = [&](CorsEntry const& rhs) {
    std::ostringstream os;
    os << rhs;
    return std::move(os).str();
  };

  auto full_input = CorsEntry{
      /*.max_age_seconds=*/3600,
      /*.method=*/{"GET", "PUT"},
      /*.origin=*/{"test-origin-1", "test-origin-2"},
      /*.response_header=*/{"response-header-1", "response-header-2"}};
  auto const full_output = to_string(full_input);
  EXPECT_THAT(full_output, HasSubstr("max_age_seconds=3600"));
  EXPECT_THAT(full_output, HasSubstr("method=[GET, PUT]"));
  EXPECT_THAT(full_output, HasSubstr("origin=[test-origin-1, test-origin-2]"));
  EXPECT_THAT(
      full_output,
      HasSubstr("response_header=[response-header-1, response-header-2]"));

  auto empty_input = CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                               /*.method=*/{},
                               /*.origin=*/{},
                               /*.response_header=*/{}};
  auto const empty_output = to_string(empty_input);
  EXPECT_THAT(empty_output, Not(HasSubstr("max_age_seconds=")));
  EXPECT_THAT(empty_output, HasSubstr("method=[]"));
  EXPECT_THAT(empty_output, HasSubstr("origin=[]"));
  EXPECT_THAT(empty_output, HasSubstr("response_header=[]"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
