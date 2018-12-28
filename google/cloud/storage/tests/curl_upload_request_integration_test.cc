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
  return google::cloud::internal::GetEnv("HTTPBIN_ENDPOINT")
      .value_or("https://nghttp2.org/httpbin");
}

TEST(CurlUploadRequestTest, UploadPartial) {
  CurlRequestBuilder builder(HttpBinEndpoint() + "/post",
                             storage::internal::GetDefaultCurlHandleFactory());
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.SetMethod("POST");
  CurlUploadRequest upload = builder.BuildUpload();

  // A small function to generate random data.
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto generate_random_data = [&generator] {
    std::string data;
    std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 /\\,;-+=";
    for (std::size_t i = 0; i != 64; ++i) {
      data += google::cloud::internal::Sample(generator, 127, characters);
      data += "\n";
    }
    return data;
  };

  // First send a full copy of the random blob.
  std::string current_message = generate_random_data();
  // Accumulate the expected data.
  auto expected_data = current_message;
  upload.NextBuffer(current_message);
  upload.Flush();

  // Send a portion of the random blob, to test shorter messages.
  current_message = generate_random_data().substr(0, 1000);
  expected_data += current_message;
  upload.NextBuffer(current_message);
  upload.Flush();

  // Test that sending messages without flushing works.
  current_message = generate_random_data();
  expected_data += current_message;
  upload.NextBuffer(current_message);

  // And test that closing after sending some data works.
  current_message = generate_random_data();
  expected_data += current_message;
  upload.NextBuffer(current_message);
  auto response = upload.Close();
  ASSERT_TRUE(response.ok());
  ASSERT_EQ(200, response->status_code)
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

  nl::json parsed = nl::json::parse(response->payload);
  // headers contains the headers that the httpbin server received, use that
  // to verify we configured CURL properly.
  auto headers = parsed["headers"];
  EXPECT_EQ("100-continue", headers.value("Expect", "")) << parsed.dump(4);

  // Verify the server received the right data.
  auto actual = parsed.value("data", "");
  // A common failure mode is to get empty data, in that case printing the delta
  // in EXPECT_EQ() is just distracting.
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_data, parsed.value("data", ""));
}

}  // namespace

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
