// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/strings/string_view.h"
#include <google/storage/v2/storage.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

auto constexpr kQuick = "The quick brown fox jumps over the lazy dog";

auto MakeTestObject() {
  auto object = google::storage::v2::Object{};
  object.set_bucket("test-bucket");
  return object;
}

TEST(ReadPayload, Basic) {
  auto const actual = ReadPayloadImpl::Make(absl::Cord(kQuick));
  EXPECT_THAT(actual.contents(), ElementsAre(absl::string_view(kQuick)));
  EXPECT_THAT(actual.size(), absl::string_view(kQuick).size());
  EXPECT_FALSE(actual.metadata().has_value());
  EXPECT_THAT(actual.headers(), IsEmpty());
  EXPECT_EQ(actual.offset(), 0);
}

TEST(ReadPayload, FromString) {
  auto const actual = ReadPayloadImpl::Make(std::string(kQuick));
  EXPECT_THAT(actual.contents(), ElementsAre(absl::string_view(kQuick)));
  EXPECT_THAT(actual.size(), absl::string_view(kQuick).size());
  EXPECT_FALSE(actual.metadata().has_value());
  EXPECT_THAT(actual.headers(), IsEmpty());
  EXPECT_EQ(actual.offset(), 0);
}

TEST(ReadPayload, FromVector) {
  auto const actual = storage_experimental::ReadPayload(
      std::vector<std::string>({std::string(kQuick), std::string(kQuick)}));
  EXPECT_THAT(actual.contents(), ElementsAre(absl::string_view(kQuick),
                                             absl::string_view(kQuick)));
}

TEST(ReadPayload, Modifiers) {
  auto const resource = MakeTestObject();
  auto const actual = ReadPayloadImpl::Make(absl::Cord(kQuick))
                          .set_metadata(resource)
                          .set_headers({{"k1", "v1"}, {"k2", "v2"}})
                          .set_offset(12345);
  EXPECT_THAT(actual.contents(), ElementsAre(absl::string_view(kQuick)));
  EXPECT_THAT(actual.size(), absl::string_view(kQuick).size());
  EXPECT_THAT(actual.metadata(), Optional(IsProtoEqual(resource)));
  EXPECT_THAT(actual.headers(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
  EXPECT_EQ(actual.offset(), 12345);
}

TEST(ReadPayload, Reset) {
  auto const actual = ReadPayloadImpl::Make(absl::Cord(kQuick))
                          .set_metadata(MakeTestObject())
                          .reset_metadata()
                          .set_headers({{"k1", "v1"}, {"k2", "v2"}})
                          .clear_headers();
  EXPECT_THAT(actual.contents(), ElementsAre(absl::string_view(kQuick)));
  EXPECT_THAT(actual.size(), absl::string_view(kQuick).size());
  EXPECT_FALSE(actual.metadata().has_value());
  EXPECT_THAT(actual.headers(), IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
