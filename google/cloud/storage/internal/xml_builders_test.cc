// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/xml_builders.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

constexpr auto kExpectedCompleteMultipartUploadXml =
    R"xml(<CompleteMultipartUpload>
  <Part>
    <PartNumber>
      2
    </PartNumber>
    <ETag>
      "7778aef83f66abc1fa1e8477f296d394"
    </ETag>
  </Part>
  <Part>
    <PartNumber>
      5
    </PartNumber>
    <ETag>
      "aaaa18db4cc2f85cedef654fccc4a4x8"
    </ETag>
  </Part>
</CompleteMultipartUpload>
)xml";

TEST(BuildCompleteMultipartUploadXmlTest, Build) {
  std::map<unsigned int, std::string> parts{
      {5U, "\"aaaa18db4cc2f85cedef654fccc4a4x8\""},
      {2U, "\"7778aef83f66abc1fa1e8477f296d394\""}};
  auto xml = CompleteMultipartUpload(parts);
  ASSERT_TRUE(xml);
  EXPECT_EQ(xml->ToString(2), kExpectedCompleteMultipartUploadXml);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
