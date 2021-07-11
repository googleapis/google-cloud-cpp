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

#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string Format(HashValues const& values) {
  if (values.md5.empty()) return values.crc32c;
  if (values.crc32c.empty()) return values.md5;
  return absl::StrCat("crc32c=", values.crc32c, ", md5=", values.md5);
}

HashValues Merge(HashValues a, HashValues b) {
  if (a.md5.empty()) a.md5 = std::move(b.md5);
  if (a.crc32c.empty()) a.crc32c = std::move(b.crc32c);
  return a;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
