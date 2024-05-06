// Copyright 2019 Google LLC
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

#include "google/cloud/internal/curl_handle.h"
#include "google/cloud/log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::HasSubstr;

TEST(CurlHandleTest, AsStatus) {
  struct {
    CURLcode curl;
    StatusCode expected;
  } expected_codes[]{
      {CURLE_OK, StatusCode::kOk},
      {CURLE_RECV_ERROR, StatusCode::kUnavailable},
      {CURLE_SEND_ERROR, StatusCode::kUnavailable},
      {CURLE_PARTIAL_FILE, StatusCode::kUnavailable},
      {CURLE_SSL_CONNECT_ERROR, StatusCode::kUnavailable},
      {CURLE_COULDNT_RESOLVE_HOST, StatusCode::kUnavailable},
      {CURLE_COULDNT_RESOLVE_PROXY, StatusCode::kUnavailable},
      {CURLE_COULDNT_CONNECT, StatusCode::kUnavailable},
      {CURLE_REMOTE_ACCESS_DENIED, StatusCode::kPermissionDenied},
      {CURLE_OPERATION_TIMEDOUT, StatusCode::kDeadlineExceeded},
      {CURLE_RANGE_ERROR, StatusCode::kUnimplemented},
      {CURLE_BAD_DOWNLOAD_RESUME, StatusCode::kInvalidArgument},
      {CURLE_ABORTED_BY_CALLBACK, StatusCode::kAborted},
      {CURLE_REMOTE_FILE_NOT_FOUND, StatusCode::kNotFound},
      {CURLE_FAILED_INIT, StatusCode::kUnknown},
      {CURLE_FTP_PORT_FAILED, StatusCode::kUnknown},
      {CURLE_GOT_NOTHING, StatusCode::kUnavailable},
      {CURLE_AGAIN, StatusCode::kUnknown},
#if CURL_AT_LEAST_VERSION(7, 43, 0)
      {CURLE_HTTP2, StatusCode::kUnavailable},
#endif  // CURL_AT_LEAST_VERSION
  };

  for (auto const& codes : expected_codes) {
    auto const expected = Status(codes.expected, "ignored");
    auto const actual = CurlHandle::AsStatus(codes.curl, "in-test");
    EXPECT_EQ(expected.code(), actual.code()) << "CURL code=" << codes.curl;
    if (!actual.ok()) {
      EXPECT_THAT(actual.message(), HasSubstr("in-test"));
      EXPECT_THAT(actual.message(), HasSubstr(curl_easy_strerror(codes.curl)));
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
