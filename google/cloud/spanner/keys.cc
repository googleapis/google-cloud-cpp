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
#include <google/protobuf/util/message_differencer.h>
#include <google/spanner/v1/keys.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {
// Appends the values in the given `key` to the `lv` proto.
void AppendKey(google::protobuf::ListValue& lv, Key&& key) {
  for (auto& v : key) {
    *lv.add_values() = spanner_internal::ToProto(std::move(v)).second;
  }
}
}  // namespace

bool operator==(KeyBound const& a, KeyBound const& b) {
  return a.key_ == b.key_ && a.bound_ == b.bound_;
}

KeySet& KeySet::AddKey(Key key) {
  if (proto_.all()) return *this;
  AppendKey(*proto_.add_keys(), std::move(key));
  return *this;
}

KeySet& KeySet::AddRange(KeyBound start, KeyBound end) {
  if (proto_.all()) return *this;
  auto* range = proto_.add_ranges();
  auto* start_proto = start.bound() == KeyBound::Bound::kClosed
                          ? range->mutable_start_closed()
                          : range->mutable_start_open();
  AppendKey(*start_proto, std::move(start).key());
  auto* end_proto = end.bound() == KeyBound::Bound::kClosed
                        ? range->mutable_end_closed()
                        : range->mutable_end_open();
  AppendKey(*end_proto, std::move(end).key());
  return *this;
}

bool operator==(KeySet const& a, KeySet const& b) {
  google::protobuf::util::MessageDifferencer differencer;
  return differencer.Compare(a.proto_, b.proto_);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
