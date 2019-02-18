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

#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

std::string HttpBinEndpoint() {
  return google::cloud::internal::GetEnv("HTTPBIN_ENDPOINT")
      .value_or("https://nghttp2.org/httpbin");
}

TEST(CurlDownloadRequestTest, SimpleStream) {
  // httpbin can generate up to 100 lines, do not try to download more than
  // that.
  constexpr int kDownloadedLines = 100;
  storage::internal::CurlRequestBuilder request(
      HttpBinEndpoint() + "/stream/" + std::to_string(kDownloadedLines),
      storage::internal::GetDefaultCurlHandleFactory());

  auto download = request.BuildDownloadRequest(std::string{});

  StatusOr<HttpResponse> response;
  std::string buffer;
  // The type for std::count() is hard to guess, most likely it is
  // std::ptrdiff_t, but could be something else, just use the aliases defined
  // for that purpose.
  std::iterator_traits<std::string::iterator>::difference_type count = 0;
  do {
    response = download.GetMore(buffer);
    EXPECT_STATUS_OK(response);
    count += std::count(buffer.begin(), buffer.end(), '\n');
  } while (response->status_code == 100);

  EXPECT_EQ(200, response->status_code)
      << ", status_code=" << response->status_code
      << ", payload=" << response->payload << ", headers={" << [&response] {
           std::string result;
           char const* sep = "";
           for (auto&& kv : response->headers) {
             result += sep;
             result += kv.first;
             result += "=";
             result += kv.second;
             sep = ", ";
           }
           result += "}";
           return result;
         }();

  EXPECT_EQ(kDownloadedLines, count);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
