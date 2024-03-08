// Copyright 2024 Google LLC
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

#include "google/cloud/spanner/proto_message.h"
#include "google/cloud/spanner/testing/singer.pb.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::HasSubstr;

testing::SingerInfo TestSinger() {
  testing::SingerInfo singer;
  singer.set_singer_id(1);
  singer.set_birth_date("1817-05-25");
  singer.set_nationality("French");
  singer.set_genre(testing::Genre::FOLK);
  return singer;
}

TEST(ProtoMessage, TypeName) {
  EXPECT_EQ(ProtoMessage<testing::SingerInfo>::TypeName(),
            "google.cloud.spanner.testing.SingerInfo");
}

TEST(ProtoMessage, DefaultValue) {
  ProtoMessage<testing::SingerInfo> msg;
  EXPECT_THAT(testing::SingerInfo(msg),
              IsProtoEqual(testing::SingerInfo::default_instance()));
}

TEST(ProtoMessage, RoundTrip) {
  auto const singer = TestSinger();
  ProtoMessage<testing::SingerInfo> msg(singer);
  EXPECT_THAT(testing::SingerInfo(msg), IsProtoEqual(singer));
}

TEST(ProtoMessage, OutputStream) {
  std::ostringstream ss;
  ss << ProtoMessage<testing::SingerInfo>(TestSinger());
  auto s = ss.str();
  EXPECT_THAT(s, HasSubstr("google.cloud.spanner.testing.SingerInfo"));
  EXPECT_THAT(s, HasSubstr("singer_id: 1"));
  EXPECT_THAT(s, HasSubstr(R"(birth_date: "1817-05-25")"));
  EXPECT_THAT(s, HasSubstr(R"(nationality: "French")"));
  EXPECT_THAT(s, HasSubstr("genre: FOLK"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
