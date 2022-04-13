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

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using google::cloud::internal::Base64Decoder;
using google::cloud::pubsub::Message;
using google::cloud::pubsublite::v1::AttributeValues;
using google::cloud::pubsublite::v1::PubSubMessage;

StatusOr<PubSubMessage> DefaultPublishMessageTransformer(
    Message const& message) {
  PubSubMessage m;
  m.set_key(message.ordering_key());
  m.set_data(std::string{message.data()});  // Handle PubsubMessageDataType by
                                            // forcing conversion to std::string
  for (auto const& kv : message.attributes()) {
    if (kv.first == kEventTimestampAttribute) {
      Base64Decoder decoder{kv.second};
      bool valid = m.mutable_event_time()->ParseFromString(
          std::string{decoder.begin(), decoder.end()});
      if (!valid) {
        return Status{StatusCode::kInvalidArgument,
                      "Not able to parse event time."};
      }
    } else {
      AttributeValues av;
      av.add_values(std::move(kv.second));
      (*m.mutable_attributes())[kv.first].add_values(kv.second);
    }
  }
  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
