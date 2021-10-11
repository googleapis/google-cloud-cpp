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

#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(GrpcUploadSessionTest, SimpleEncodeDecode) {
  ResumableUploadSessionGrpcParams input{"some-upload-id"};
  auto encoded = EncodeGrpcResumableUploadSessionUrl(input);
  ASSERT_TRUE(IsGrpcResumableSessionUrl(encoded));
  auto decoded = DecodeGrpcResumableUploadSessionUrl(encoded);
  ASSERT_STATUS_OK(decoded) << "Failed to decode url: " << encoded;
  EXPECT_EQ(input.upload_id, decoded->upload_id);
}

TEST(GrpcUploadSessionTest, MalformedUri) {
  EXPECT_FALSE(IsGrpcResumableSessionUrl(""));
  EXPECT_FALSE(IsGrpcResumableSessionUrl("grpc:/"));
  EXPECT_FALSE(IsGrpcResumableSessionUrl("https://somerubbish"));
  auto res = DecodeGrpcResumableUploadSessionUrl("");
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(), HasSubstr("different implementation"));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
