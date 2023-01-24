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
#include "google/cloud/storage/internal/xml_parser_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

constexpr auto kXmlFilledWithGarbage =
    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE lolz [
    <!ENTITY lol "lol">
    <!ENTITY lol2 "&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;">
    ]>
<!-- this is a comment -->
<InitiateMultipartUploadResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <!--
  this is a multiline comment
  -->
  <Bucket>travel-maps</Bucket>
  <![CDATA[
    This is CDATA text.
  ]]>
  <Key>paris.jpg</Key>
  <UploadId>VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA</UploadId>
</InitiateMultipartUploadResult>
)xml";

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

TEST(XmlNodeTest, Parse) {
  auto res = XmlNode::Parse(kXmlFilledWithGarbage);
  EXPECT_TRUE(res.ok());
  auto rendered = res.value()->ToString(2);
  EXPECT_EQ(rendered, kExpectedXml);
  // It should be parsable again.
  res = XmlNode::Parse(rendered);
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(res.value()->ToString(2), kExpectedXml);
}

TEST(XmlNodeTest, ContentTooLarge) {
  Options options;
  options.set<XmlParserMaxSourceSize>(10);
  auto res = XmlNode::Parse(kXmlFilledWithGarbage, options);
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("exceeds the max size")));
}

TEST(XmlNodeTest, MaxNodeCount) {
  Options options;
  options.set<XmlParserMaxNodeCount>(1);
  auto res = XmlNode::Parse("<tag></tag>", options);
  EXPECT_TRUE(res.ok());
  res = XmlNode::Parse("<tag>It exceeds the limit</tag>", options);
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("Exceeding max node count")));
}

TEST(XmlNodeTest, MaxNodeDepth) {
  Options options;
  options.set<XmlParserMaxNodeDepth>(1);
  auto res = XmlNode::Parse("<tag></tag>", options);
  EXPECT_TRUE(res.ok());
  res = XmlNode::Parse("<tag>It exceeds the limit</tag>", options);
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("Exceeding max node depth")));
}

TEST(XmlNodeTest, BrokenXmlDeclaration) {
  auto res = XmlNode::Parse("<?xml");
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("XML declaration")));
}

TEST(XmlNodeTest, MissMatchedTag) {
  auto res = XmlNode::Parse("<tag></nottag>");
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("Mismatched close tag")));
}

TEST(XmlNodeTest, TagNotClosed) {
  auto res = XmlNode::Parse("<tag/");
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("tag never closes")));
}

TEST(XmlNodeTest, InvalidTag) {
  auto res = XmlNode::Parse("</>");
  EXPECT_THAT(res,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid tag")));
}

constexpr auto kExpectedCompleteMultipartUpload =
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

TEST(XmlNodeTest, CompleteMultipartUpload) {
  std::map<std::size_t, std::string> parts{
      {5, "\"aaaa18db4cc2f85cedef654fccc4a4x8\""},
      {2, "\"7778aef83f66abc1fa1e8477f296d394\""}};
  auto xml = XmlNode::CompleteMultipartUpload(parts);
  ASSERT_NE(xml, nullptr);
  EXPECT_EQ(xml->ToString(2), kExpectedCompleteMultipartUpload);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
