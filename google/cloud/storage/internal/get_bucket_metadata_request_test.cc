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

#include "google/cloud/storage/internal/get_bucket_metadata_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;
using ::testing::Not;

TEST(GetBucketMetadataRequestTest, OStreamBasic) {
  GetBucketMetadataRequest request("my-bucket");
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
}

TEST(GetBucketMetadataRequestTest, OStreamParameter) {
  GetBucketMetadataRequest request("my-bucket");
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
