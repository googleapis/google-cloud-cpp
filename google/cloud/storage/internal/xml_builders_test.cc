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

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

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

TEST(CompleteMultipartUploadXmlBuilderTest, Build) {
  auto builder = CompleteMultipartUploadXmlBuilder::Create();
  // The parts should be sorted by part_number in the final xml.
  auto res = builder->AddPart(5, "\"aaaa18db4cc2f85cedef654fccc4a4x8\"");
  EXPECT_TRUE(res.ok());
  builder->AddPart(2, "old value");  // This should be replaced.
  res = builder->AddPart(2, "\"7778aef83f66abc1fa1e8477f296d394\"");
  EXPECT_TRUE(res.ok());
  auto xml = builder->Build();
  EXPECT_EQ(xml->ToString(2), kExpectedCompleteMultipartUploadXml);
}

TEST(CompleteMultipartUploadXmlBuilderTest, InvalidInput) {
  auto builder = CompleteMultipartUploadXmlBuilder::Create();
  auto res = builder->AddPart(0, "\"7778aef83f66abc1fa1e8477f296d394\"");
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("can not be zero")));
  res = builder->AddPart(kMaxPartNumber + 1,
                         "\"7778aef83f66abc1fa1e8477f296d394\"");
  EXPECT_THAT(res, StatusIs(StatusCode::kInvalidArgument,
                            HasSubstr("can not be more than")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
