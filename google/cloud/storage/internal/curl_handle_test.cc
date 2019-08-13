// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/curl_handle.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
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
      {CURLE_AGAIN, StatusCode::kUnknown},
  };

  for (auto const& codes : expected_codes) {
    auto const expected = Status(codes.expected, "ignored");
    auto const actual = CurlHandle::AsStatus(codes.curl, "in-test");
    EXPECT_EQ(expected.code(), actual.code());
    if (!actual.ok()) {
      EXPECT_THAT(actual.message(), HasSubstr("in-test"));
      EXPECT_THAT(actual.message(), HasSubstr(curl_easy_strerror(codes.curl)));
    }
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
