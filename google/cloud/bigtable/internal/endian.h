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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDIAN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDIAN_H_

#include "google/cloud/bigtable/internal/encoder.h"
#include "google/cloud/bigtable/internal/strong_type.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
using bigendian64_t = internal::StrongType<std::int64_t, struct BigEndianType>;

namespace internal {

template <>
struct Encoder<bigtable::bigendian64_t> {
  static std::string Encode(bigtable::bigendian64_t const& value);
  static bigtable::bigendian64_t Decode(std::string const& value);
};

bigtable::bigendian64_t ByteSwap64(bigtable::bigendian64_t value);
std::string AsBigEndian64(bigtable::bigendian64_t value);

}  // namespace internal

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDIAN_H_
