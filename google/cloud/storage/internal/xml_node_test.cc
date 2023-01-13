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

TEST(XmlNodeTest, EscapeXmlTag) {
  std::vector<std::pair<std::string, std::string>> cases = {
      {"\"Google\"", "&quot;Google&quot;"},
      {"'Google'", "&apos;Google&apos;"},
      {"<html>", "&lt;html&gt;"},
      {"Bread&Butter", "Bread&amp;Butter"},
  };
  for (auto& p : cases) {
    auto after = EscapeXmlTag(p.first);
    EXPECT_EQ(after, p.second);
  }
}

TEST(XmlNodeTest, EscapeXmlContent) {
  std::vector<std::pair<std::string, std::string>> cases = {
      {"\"Google\"", "\"Google\""},
      {"'Google'", "'Google'"},
      {"<html>", "&lt;html&gt;"},
      {"Bread&Butter", "Bread&amp;Butter"},
  };
  for (auto& p : cases) {
    auto after = EscapeXmlContent(p.first);
    EXPECT_EQ(after, p.second);
  }
}

TEST(XmlNodeTest, BuildTree) {
  auto root = XmlNode::CreateRoot();
  auto mpu_result = root->AppendTagNode("InitiateMultipartUploadResult");
  auto bucket = mpu_result->AppendTagNode("Bucket");
  bucket->AppendTextNode("travel-maps");
  auto key = mpu_result->AppendTagNode("Key");
  key->AppendTextNode("paris.jpg");
  auto upload_id = mpu_result->AppendTagNode("UploadId");
  upload_id->AppendTextNode(
      "VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA");
  EXPECT_EQ(root->ToString(2), kExpectedXml);
  auto children = root->GetChildren();
  EXPECT_THAT(children, ElementsAre(mpu_result));
  auto tags = mpu_result->GetChildren("UploadId");
  EXPECT_THAT(tags, ElementsAre(upload_id));
  tags = mpu_result->GetChildren();
  EXPECT_THAT(tags, ElementsAre(bucket, key, upload_id));
}

TEST(XmlNodeTest, Accessors) {
  auto root = XmlNode::CreateRoot();
  auto text_node = root->AppendTextNode("text");
  EXPECT_EQ(text_node->GetConcatenatedText(), "text");
  EXPECT_EQ(text_node->GetTextContent(), "text");
  EXPECT_EQ(text_node->GetTagName(), "");

  auto tag_node = root->AppendTagNode("Tag");
  tag_node->AppendTextNode("tag text");
  EXPECT_EQ(tag_node->GetTextContent(), "");
  EXPECT_EQ(tag_node->GetConcatenatedText(), "tag text");
  EXPECT_EQ(tag_node->GetTagName(), "Tag");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
