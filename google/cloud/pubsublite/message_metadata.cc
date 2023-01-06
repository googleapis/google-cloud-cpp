// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/message_metadata.h"

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<MessageMetadata> MakeMessageMetadata(std::string const& input) {
  std::vector<std::string> splits = absl::StrSplit(input, ':');
  std::int64_t partition;
  std::int64_t offset;
  if (splits.size() == 2 && absl::SimpleAtoi(splits[0], &partition) &&
      absl::SimpleAtoi(splits[1], &offset)) {
    google::cloud::pubsublite::v1::Cursor cursor;
    cursor.set_offset(offset);
    return MessageMetadata{partition, std::move(cursor)};
  }
  return Status{StatusCode::kInvalidArgument,
                "Not able to parse `MessageMetadata`"};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
