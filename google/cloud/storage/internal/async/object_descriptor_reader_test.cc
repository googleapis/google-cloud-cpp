// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/object_descriptor_reader.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_experimental::ReadPayload;
using ::google::protobuf::TextFormat;
using ::testing::ElementsAre;
using ::testing::ResultOf;
using ::testing::VariantWith;

TEST(ObjectDescriptorReader, Basic) {
  auto impl = std::make_shared<ReadRange>(10000, 30);
  auto tested = ObjectDescriptorReader(impl);

  auto data = google::storage::v2::ObjectRangeData{};
  auto constexpr kData0 = R"pb(
    checksummed_data { content: "0123456789" }
    read_range { read_offset: 10000 read_limit: 10 read_id: 7 }
    range_end: false
  )pb";
  EXPECT_TRUE(TextFormat::ParseFromString(kData0, &data));
  impl->OnRead(std::move(data));

  auto actual = tested.Read().get();
  EXPECT_THAT(actual,
              VariantWith<ReadPayload>(ResultOf(
                  "contents", [](ReadPayload const& p) { return p.contents(); },
                  ElementsAre("0123456789"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
