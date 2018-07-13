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

#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <vector>

using ::testing::HasSubstr;

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {
std::string HttpBinEndpoint() {
  auto env = std::getenv("HTTPBIN_ENDPOINT");
  if (env != nullptr) {
    return env;
  }
  return "https://nghttp2.org/httpbin";
}

TEST(CurlUploadRequestTest, UploadPartial) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/post");
  builder.AddHeader("Content-Type: application/octet-stream");
  CurlUploadRequest upload = builder.BuildUpload();

  // Create some random data to upload
  std::string msg;
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 /\\,;-+=";
  for (std::size_t i = 0; i != 64; ++i) {
    msg += google::cloud::internal::Sample(generator, 127, characters);
    msg += "\n";
  }

  std::string copy = msg;
  auto expected_data = copy;

  upload.NextBuffer(copy);
  upload.Flush();
  copy = msg.substr(0, 1000);
  expected_data += copy;
  upload.NextBuffer(copy);
  upload.Flush();
  copy = msg;
  expected_data += copy;
  upload.NextBuffer(copy);
  copy = msg;
  expected_data += copy;
  upload.NextBuffer(copy);
  auto response = upload.Close();
  EXPECT_EQ(200, response.status_code);

  nl::json parsed = nl::json::parse(response.payload);
  // headers contains the headers that the httpbin server received, use that
  // to verify we configured CURL properly.
  auto headers = parsed["headers"];
  EXPECT_EQ("100-continue", headers.value("Expect", "")) << parsed.dump(4);

  // Verify the server received the right data.
  EXPECT_EQ(expected_data, parsed.value("data", ""));
}

}  // namespace

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
