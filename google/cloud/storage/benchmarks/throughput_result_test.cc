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

#include "google/cloud/storage/benchmarks/throughput_result.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

using ::testing::EndsWith;
using ::testing::HasSubstr;

MATCHER_P(
    HasQuotedStatus, substr,
    "status field from PrintAsCsv is properly quoted and contains substr") {
  static auto count = [] {
    std::ostringstream os;
    PrintThroughputResultHeader(os);
    auto const h = std::move(os).str();
    return std::count(h.begin(), h.end(), ',');
  }();

  // The status field is the 10th value.
  std::size_t pos = 0;
  for (int i = 0; i != count; ++i) {
    pos = arg.find(',', pos);
    if (pos == std::string::npos) {
      *result_listener << "Couldn't find status field: " << arg;
      return false;
    }
    ++pos;
  }
  std::string status = arg.substr(pos);
  if (status.find(substr) == std::string::npos) {
    *result_listener << "Didn't find " << substr << " in " << status;
    return false;
  }
  return true;
}

StatusOr<std::string> ToString(ThroughputResult const& result) {
  std::ostringstream os;
  PrintAsCsv(os, result);
  if (!os) {
    return Status{StatusCode::kInternal, "PrintAsCsv failed"};
  };
  return os.str();
}

TEST(ThroughputResult, HeaderMatches) {
  std::ostringstream header_stream;
  PrintThroughputResultHeader(header_stream);
  EXPECT_TRUE(header_stream);
  auto const header = std::move(header_stream).str();

  auto const line = ToString(ThroughputResult{
      ExperimentLibrary::kCppClient, ExperimentTransport::kGrpc, kOpInsert,
      /*object_size=*/5 * kMiB, /*transfer_size=*/3 * kMiB,
      /*app_buffer_size=*/2 * kMiB, /*lib_buffer_size=*/4 * kMiB,
      /*crc_enabled=*/true, /*md5_enabled=*/false,
      std::chrono::microseconds(234000), std::chrono::microseconds(345000),
      Status{StatusCode::kOutOfRange, "OOR-status-message"}, "peer"});
  ASSERT_STATUS_OK(line);
  ASSERT_FALSE(header.empty());
  ASSERT_FALSE(line->empty());

  auto const header_fields = std::count(header.begin(), header.end(), ',');
  auto const line_fields = std::count(line->begin(), line->end(), ',');
  EXPECT_EQ(header_fields, line_fields);
  EXPECT_EQ(line->back(), '\n');
  EXPECT_EQ(header.back(), '\n');
  EXPECT_THAT(header, EndsWith(",Status\n"));

  // We don't want to create a change detector test, but we can verify the basic
  // fields are formatted correctly.
  EXPECT_THAT(*line, HasSubstr(ToString(ExperimentLibrary::kCppClient)));
  EXPECT_THAT(*line, HasSubstr(ToString(ExperimentTransport::kGrpc)));
  EXPECT_THAT(*line, HasSubstr(ToString(kOpInsert)));
  EXPECT_THAT(*line, HasSubstr("," + std::to_string(5 * kMiB) + ","));
  EXPECT_THAT(*line, HasSubstr("," + std::to_string(3 * kMiB) + ","));
  EXPECT_THAT(*line, HasSubstr("," + std::to_string(2 * kMiB) + ","));
  EXPECT_THAT(*line, HasSubstr("," + std::to_string(4 * kMiB) + ","));
  EXPECT_THAT(*line, HasSubstr(",1,"));  // crc_enabled==true
  EXPECT_THAT(*line, HasSubstr(",0,"));  // md5_enabled==false
  EXPECT_THAT(*line, HasSubstr(",234000,"));
  EXPECT_THAT(*line, HasSubstr(",345000,"));
  EXPECT_THAT(*line, HasSubstr(StatusCodeToString(StatusCode::kOutOfRange)));
  EXPECT_THAT(*line, HasSubstr("OOR-status-message"));
  EXPECT_THAT(*line, HasSubstr("peer"));
}

TEST(ThroughputResult, QuoteCsv) {
  ThroughputResult result{};

  result.status = Status{StatusCode::kInternal, R"(message "with quotes")"};
  auto line = ToString(result);
  ASSERT_STATUS_OK(line);
  EXPECT_THAT(*line, HasQuotedStatus(R"(message "with quotes")"));

  result.status = Status{StatusCode::kInternal, R"(message, with comma)"};
  line = ToString(result);
  ASSERT_STATUS_OK(line);
  EXPECT_THAT(*line, HasQuotedStatus(R"(message; with comma)"));

  result.status = Status{StatusCode::kInternal, "message\nwith newline"};
  line = ToString(result);
  ASSERT_STATUS_OK(line);
  EXPECT_THAT(*line, HasQuotedStatus("message;with newline"));

  result.status = Status{StatusCode::kInternal, "message\r\nwith CRLF"};
  line = ToString(result);
  ASSERT_STATUS_OK(line);
  EXPECT_THAT(*line, HasQuotedStatus("message;;with CRLF"));

  result.status =
      Status{StatusCode::kInternal, R"("message, "with quotes", commas,)"
                                    "\r\nand CRLF"};
  line = ToString(result);
  ASSERT_STATUS_OK(line);
  EXPECT_THAT(*line, HasQuotedStatus(R"(message; "with quotes"; commas;)"
                                     ";;and CRLF"));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
