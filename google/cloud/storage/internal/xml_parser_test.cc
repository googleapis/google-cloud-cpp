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

#include "google/cloud/storage/internal/xml_parser.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

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
  <Key><b>p</b>ari<b>s</b>.jpg</Key>
  <UploadId>VXBsb2FkIElEIGZvciBlbHZpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA</UploadId>
</InitiateMultipartUploadResult>
)xml";

TEST(XmlParserTest, CleanupXml) {
  auto parser = XmlParser::Create();
  auto after = parser->CleanUpXml(kXmlFilledWithGarbage);
  // These elements should be removed.
  EXPECT_THAT(after, Not(HasSubstr("<?xml")));
  EXPECT_THAT(after, Not(HasSubstr("CDATA")));
  EXPECT_THAT(after, Not(HasSubstr("!DOCTYPE")));
  EXPECT_THAT(after, Not(HasSubstr("<!--")));
  // The Tags should be preserved.
  EXPECT_THAT(after, HasSubstr("InitiateMultipartUploadResult"));
  EXPECT_THAT(after, HasSubstr("Bucket"));
  EXPECT_THAT(after, HasSubstr("Key"));
  EXPECT_THAT(after, HasSubstr("UploadId"));
}

TEST(XmlParserTest, ParseXml) {
  auto parser = XmlParser::Create();
  auto res = parser->ParseXml(kXmlFilledWithGarbage);
  EXPECT_THAT(res, StatusIs(StatusCode::kUnimplemented));
}

TEST(XmlParserTest, TooLarge) {
  auto parser = XmlParser::Create();
  Options options;
  options.set<XmlParserSourceMaxBytes>(10);
  auto res = parser->ParseXml(kXmlFilledWithGarbage, options);
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("exceeds the max byte")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
