// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

std::string HttpBinEndpoint() {
  return google::cloud::internal::GetEnv("HTTPBIN_ENDPOINT")
      .value_or("https://nghttp2.org/httpbin");
}

TEST(CurlDownloadRequestTest, SimpleStream) {
  // httpbin can generate up to 100 lines, do not try to download more than
  // that.
  constexpr int kDownloadedLines = 100;

  std::size_t count = 0;
  auto download = [&] {
    count = 0;
    CurlRequestBuilder request(
        HttpBinEndpoint() + "/stream/" + std::to_string(kDownloadedLines),
        storage::internal::GetDefaultCurlHandleFactory());
    auto download = request.BuildDownloadRequest(std::string{});
    char buffer[128 * 1024];
    do {
      auto n = sizeof(buffer);
      auto result = download.Read(buffer, n);
      if (!result) return std::move(result).status();
      if (result->bytes_received > sizeof(buffer)) {
        return Status{StatusCode::kUnknown, "invalid byte count"};
      }
      count += static_cast<std::size_t>(
          std::count(buffer, buffer + result->bytes_received, '\n'));
      if (result->response.status_code != 100) break;
    } while (true);
    return Status{};
  };

  auto delay = std::chrono::seconds(1);
  for (int i = 0; i != 3; ++i) {
    auto result = download();
    if (result.ok()) break;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  EXPECT_EQ(kDownloadedLines, count);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
