// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/snapshot.h"

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::string Snapshot::FullName() const {
  return "projects/" + project_id() + "/snapshots/" + snapshot_id();
}

bool operator==(Snapshot const& a, Snapshot const& b) {
  return a.project_id() == b.project_id() && a.snapshot_id() == b.snapshot_id();
}

std::ostream& operator<<(std::ostream& os, Snapshot const& rhs) {
  return os << rhs.FullName();
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
