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

#include "google/cloud/version.h"
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <utility>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MessageMetadata {
 public:
  MessageMetadata(int64_t partition,
                  google::cloud::pubsublite::v1::Cursor cursor)
      : partition_{partition}, cursor_{std::move(cursor)} {}

  int64_t GetPartition() const { return partition_; }

  google::cloud::pubsublite::v1::Cursor GetCursor() const { return cursor_; }

 private:
  int64_t const partition_;
  google::cloud::pubsublite::v1::Cursor cursor_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_MESSAGE_METADATA_H
