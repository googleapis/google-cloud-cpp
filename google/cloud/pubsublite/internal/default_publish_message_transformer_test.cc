// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/default_publish_message_transformer.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/timestamp.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {

using ::google::cloud::testing_util::IsProtoEqual;

using google::cloud::internal::Base64Encoder;
using google::cloud::pubsub::MessageBuilder;
using google::cloud::pubsublite::v1::PubSubMessage;
using google::protobuf::Timestamp;

class DefaultPublishMessageTransformerTest : public ::testing::Test {
 protected:
  DefaultPublishMessageTransformerTest()
      : pubsub_message_{MessageBuilder{}
                            .SetData("dataaaa")
                            .SetOrderingKey("keyyyy")
                            .InsertAttribute("attr1_key", "attr1_key")
                            .InsertAttribute("lang", "cpp")},
        psl_message_{[] {
          PubSubMessage m;
          m.set_data("dataaaa");
          m.set_key("keyyyy");
          (*m.mutable_attributes())["attr1_key"].add_values("attr1_key");
          (*m.mutable_attributes())["lang"].add_values("cpp");
          return m;
        }()} {}

  MessageBuilder pubsub_message_;
  PubSubMessage psl_message_;
};

TEST_F(DefaultPublishMessageTransformerTest, NoTimestamp) {
  EXPECT_THAT(
      *DefaultPublishMessageTransformer(std::move(pubsub_message_).Build()),
      IsProtoEqual(psl_message_));
}

TEST_F(DefaultPublishMessageTransformerTest, Timestamp) {
  Timestamp t;
  t.set_seconds(42);
  t.set_nanos(42);
  std::string serialized = t.SerializeAsString();
  Base64Encoder encoder;
  for (char const c : serialized) encoder.PushBack(c);
  pubsub_message_.InsertAttribute(kEventTimestampAttribute,
                                  std::move(encoder).FlushAndPad());
  *psl_message_.mutable_event_time() = std::move(t);
  EXPECT_THAT(
      *DefaultPublishMessageTransformer(std::move(pubsub_message_).Build()),
      IsProtoEqual(psl_message_));
}

TEST_F(DefaultPublishMessageTransformerTest, InvalidTimestamp) {
  pubsub_message_.InsertAttribute(kEventTimestampAttribute, "oops");
  *psl_message_.mutable_event_time() = Timestamp{};
  EXPECT_EQ(
      DefaultPublishMessageTransformer(std::move(pubsub_message_).Build())
          .status(),
      (Status{StatusCode::kInvalidArgument, "Not able to parse event time."}));
}

}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
