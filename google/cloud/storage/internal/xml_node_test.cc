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

#include "google/cloud/storage/internal/xml_node.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;

constexpr auto kExpectedXml =
    R"xml(<InitiateMultipartUploadResult>
  <Bucket>
    travel-maps
  </Bucket>
  <Key>
    paris.jpg
  </Key>
  <UploadId>
    VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA
  </UploadId>
</InitiateMultipartUploadResult>
)xml";

TEST(XmlNodeTest, BuildTree) {
  XmlNode root;
  auto* mpu_result = root.AppendChild("InitiateMultipartUploadResult", "");
  mpu_result->AppendChild("Bucket", "")->AppendChild("", "travel-maps");
  mpu_result->AppendChild("Key", "")->AppendChild("", "paris.jpg");
  auto* upload_id_tag = mpu_result->AppendChild("UploadId", "");
  upload_id_tag->AppendChild(
      "", "VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA");
  EXPECT_EQ(root.ToString(2), kExpectedXml);
  auto children = root.GetChildren();
  EXPECT_THAT(children, ElementsAre(mpu_result));
  auto tags = mpu_result->GetChildren("UploadId");
  EXPECT_THAT(tags, ElementsAre(upload_id_tag));
}

TEST(XmlNodeTest, NonTagElement) {
  // Non-tag node just returns its text_content
  XmlNode non_tag{"", "text"};
  EXPECT_EQ(non_tag.GetConcatenatedText(), "text");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
