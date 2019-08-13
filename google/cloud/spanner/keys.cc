// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/value.h"
#include <google/spanner/v1/keys.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

::google::spanner::v1::KeySet ToProto(KeySet keyset) {
  ::google::spanner::v1::KeySet proto;
  proto.set_all(keyset.IsAll());

  auto make_key = [](KeySet::ValueRow key) {
    google::protobuf::ListValue lv;
    for (Value& v : key.mutable_column_values()) {
      std::pair<google::spanner::v1::Type, google::protobuf::Value> p =
          ToProto(std::move(v));
      *lv.add_values() = std::move(p.second);
    }
    return lv;
  };

  for (KeySet::ValueRow& key : keyset.key_values_) {
    *proto.add_keys() = make_key(std::move(key));
  }

  for (KeySet::ValueKeyRange& range : keyset.key_ranges_) {
    google::spanner::v1::KeyRange kr;
    auto& start = range.mutable_start();
    auto& end = range.mutable_end();
    if (start.mode() == KeySet::ValueBound::Mode::MODE_CLOSED) {
      *kr.mutable_start_closed() = make_key(std::move(start.mutable_key()));
    } else {
      *kr.mutable_start_open() = make_key(std::move(start.mutable_key()));
    }

    if (end.mode() == KeySet::ValueBound::Mode::MODE_CLOSED) {
      *kr.mutable_end_closed() = make_key(std::move(end.mutable_key()));
    } else {
      *kr.mutable_end_open() = make_key(std::move(end.mutable_key()));
    }

    *proto.add_ranges() = std::move(kr);
  }

  return proto;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
