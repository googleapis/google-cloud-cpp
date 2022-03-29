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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_MESSAGE_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_MESSAGE_METADATA_H

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <utility>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class stores the partition and `Cursor` of a published Pub/Sub Lite
 * message.
 */
class MessageMetadata {
 public:
  MessageMetadata(std::int64_t partition,
                  google::cloud::pubsublite::v1::Cursor cursor)
      : partition_{partition}, cursor_{std::move(cursor)} {}

  std::int64_t Partition() const { return partition_; }

  google::cloud::pubsublite::v1::Cursor Cursor() const { return cursor_; }

  /**
   * Parses a string into a MessageMetadata object. The formatting of this
   * string is not stable cross-binary.
   */
  static StatusOr<MessageMetadata> Parse(std::string const& input) {
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

  /**
   * Serializes current object. Serialization format is not stable cross-binary
   */
  std::string Serialize() const {
    return absl::StrCat(std::to_string(partition_), ":",
                        std::to_string(cursor_.offset()));
  }

 private:
  std::int64_t const partition_;
  google::cloud::pubsublite::v1::Cursor cursor_;
};

inline bool operator==(MessageMetadata const& a, MessageMetadata const& b) {
  return a.Partition() == b.Partition() &&
         a.Cursor().offset() == b.Cursor().offset();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_MESSAGE_METADATA_H
