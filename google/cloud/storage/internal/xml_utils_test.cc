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

#include "google/cloud/storage/internal/xml_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

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

TEST(XmlUtilsTest, XmlNodeTest) {
  XmlNode root;
  XmlNode child{"InitiateMultipartUploadResult", ""};
  auto* mpu_result = root.EmplaceChild(std::move(child));
  mpu_result->EmplaceChild("Bucket", "")->EmplaceChild("", "travel-maps");
  mpu_result->EmplaceChild("Key", "")->EmplaceChild("", "paris.jpg");
  mpu_result->EmplaceChild("UploadId", "")
      ->EmplaceChild(
          "", "VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA");
  EXPECT_EQ(root.ToString(2), kExpectedXml);
  auto tag1 = root.GetChild("InitiateMultipartUploadResult");
  EXPECT_TRUE(tag1.ok());
  EXPECT_EQ(tag1.value(), mpu_result);
  auto children = root.GetChildren();
  EXPECT_EQ(children.size(), 1);
  EXPECT_EQ(children[0], mpu_result);
  auto tags = mpu_result->GetChildren("UploadId");
  EXPECT_EQ(tags.size(), 1);
  EXPECT_EQ(tags[0]->GetConcatenatedText(),
            "VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
