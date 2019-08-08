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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_

#include "google/cloud/spanner/version.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The `KeySet` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index.
 *
 * @warning TODO(#202) This class is currently just a stub; it can only
 * represent "all keys" or "no keys".
 */
class KeySet {
 public:
  static KeySet All() { return KeySet(true); }
  /**
   * Constructs an empty `KeySet`.
   */
  KeySet() = default;

  /**
   * Does the `KeySet` represent all keys for an index.
   */
  bool IsAll() const { return all_; }

 private:
  explicit KeySet(bool all) : all_(all) {}

  bool all_ = false;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
