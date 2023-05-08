// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(GetBaseEndpointTest, Success) {
  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://bigquery.googleapis.com",
       "https://bigquery.googleapis.com/bigquery/v2"},
      {"http://bigquery.googleapis.com",
       "http://bigquery.googleapis.com/bigquery/v2"},
      {"bigquery.googleapis.com",
       "https://bigquery.googleapis.com/bigquery/v2"},
      {"https://bigquery.googleapis.com/",
       "https://bigquery.googleapis.com/bigquery/v2"},
      {"", ""},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);

    auto actual = GetBaseEndpoint(opts);
    EXPECT_EQ(test.expected, actual);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
