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

#include "google/cloud/storage/internal/insert_object_media_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

TEST(InsertObjectMediaRequestTest, OStream) {
  InsertObjectMediaRequest request("my-bucket", "my-object", "object contents");
  request.set_multiple_options(
      IfGenerationMatch(0), Projection("full"), ContentEncoding("media"),
      KmsKeyName("random-key"), PredefinedAcl("authenticatedRead"));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("InsertObjectMediaRequest"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("ifGenerationMatch=0"));
  EXPECT_THAT(str, HasSubstr("projection=full"));
  EXPECT_THAT(str, HasSubstr("kmsKeyName=random-key"));
  EXPECT_THAT(str, HasSubstr("contentEncoding=media"));
  EXPECT_THAT(str, HasSubstr("predefinedAcl=authenticatedRead"));
}

TEST(InsertObjectStreamingRequestTest, OStream) {
  InsertObjectStreamingRequest request("my-bucket", "my-object");
  request.set_multiple_options(
      IfGenerationMatch(0), Projection("full"), ContentEncoding("media"),
      KmsKeyName("random-key"), PredefinedAcl("authenticatedRead"));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("InsertObjectStreamingRequest"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("ifGenerationMatch=0"));
  EXPECT_THAT(str, HasSubstr("projection=full"));
  EXPECT_THAT(str, HasSubstr("kmsKeyName=random-key"));
  EXPECT_THAT(str, HasSubstr("contentEncoding=media"));
  EXPECT_THAT(str, HasSubstr("predefinedAcl=authenticatedRead"));
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
