// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASH_MISMATCH_ERROR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASH_MISMATCH_ERROR_H

#include "google/cloud/storage/version.h"
#include <ios>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {

/**
 * Report checksum mismatches as exceptions.
 */
class HashMismatchError : public std::ios_base::failure {
 public:
  explicit HashMismatchError(std::string const& msg, std::string received,
                             std::string computed)
      : std::ios_base::failure(msg),
        received_hash_(std::move(received)),
        computed_hash_(std::move(computed)) {}

  std::string const& received_hash() const { return received_hash_; }
  std::string const& computed_hash() const { return computed_hash_; }

 private:
  std::string received_hash_;
  std::string computed_hash_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_HASH_MISMATCH_ERROR_H
