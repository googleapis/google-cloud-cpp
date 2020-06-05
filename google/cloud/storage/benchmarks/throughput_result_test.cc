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

#include "google/cloud/storage/benchmarks/throughput_result.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

using ::testing::HasSubstr;

TEST(ThroughtputResult, HeaderMatches) {
  std::ostringstream header_stream;
  PrintThroughputResultHeader(header_stream);
  EXPECT_TRUE(header_stream);
  auto const header = std::move(header_stream).str();

  std::ostringstream line_stream;
  PrintAsCsv(line_stream,
             ThroughputResult{
                 kOpInsert, /*object_size=*/3 * kMiB,
                 /*app_buffer_size=*/2 * kMiB, /*lib_buffer_size=*/4 * kMiB,
                 /*crc_enabled=*/true, /*md5_enabled=*/false, ApiName::kApiGrpc,
                 std::chrono::microseconds(234000),
                 std::chrono::microseconds(345000), StatusCode::kOutOfRange});
  EXPECT_TRUE(line_stream);
  auto const line = std::move(line_stream).str();

  ASSERT_FALSE(header.empty());
  ASSERT_FALSE(line.empty());

  auto const header_fields = std::count(header.begin(), header.end(), ',');
  auto const line_fields = std::count(line.begin(), line.end(), ',');
  EXPECT_EQ(header_fields, line_fields);
  EXPECT_EQ(line.back(), '\n');
  EXPECT_EQ(header.back(), '\n');

  // We don't want to create a change detector test, but we can verify the basic
  // fields are formatted correctly.
  EXPECT_THAT(line, HasSubstr(ToString(kOpInsert)));
  EXPECT_THAT(line, HasSubstr("," + std::to_string(3 * kMiB) + ","));
  EXPECT_THAT(line, HasSubstr("," + std::to_string(2 * kMiB) + ","));
  EXPECT_THAT(line, HasSubstr("," + std::to_string(4 * kMiB) + ","));
  EXPECT_THAT(line, HasSubstr(",1,"));  // crc_enabled==true
  EXPECT_THAT(line, HasSubstr(",0,"));  // md5_enabled==false
  EXPECT_THAT(line, HasSubstr(ToString(ApiName::kApiGrpc)));
  EXPECT_THAT(line, HasSubstr(",234000,"));
  EXPECT_THAT(line, HasSubstr(",345000,"));
  EXPECT_THAT(line, HasSubstr(StatusCodeToString(StatusCode::kOutOfRange)));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
