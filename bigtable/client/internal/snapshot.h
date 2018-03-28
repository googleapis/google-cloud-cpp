// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_SNAPSHOT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_SNAPSHOT_H_

#include "bigtable/client/version.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * In-memory representation of Table snapshot.
 */
class Snapshot {
 public:
  // Create snapshot from given parameters.
  Snapshot(int64_t data_size, std::string description, std::string name) :
      data_size_(data_size), description_(std::move(description)),
      name_(std::move(name)) {}

  int64_t const& data_size() { return data_size_; }
  std::string const& description() { return description_; }
  std::string const& name() { return name_; }
 private:
  int64_t data_size_;
  std::string description_;
  std::string name_;
};
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_SNAPSHOT_H_
