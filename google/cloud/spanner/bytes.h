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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BYTES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BYTES_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/status_or.h"
#include <array>
#include <cstddef>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct BytesInternals;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A representation of the Spanner BYTES type: variable-length binary data.
 *
 * A `Bytes` value can be constructed from, and converted to any sequence of
 * octets. `Bytes` values can be compared for equality.
 */
class Bytes {
 public:
  /// An empty sequence.
  Bytes() = default;

  /// Construction from a sequence of octets.
  ///@{
  template <typename InputIt>
  Bytes(InputIt first, InputIt last) {
    google::cloud::internal::Base64Encoder encoder;
    while (first != last) encoder.PushBack(*first++);
    base64_rep_ = std::move(encoder).FlushAndPad();
  }
  template <typename Container>
  explicit Bytes(Container const& c) : Bytes(std::begin(c), std::end(c)) {}
  ///@}

  /// Conversion to a sequence of octets.  The `Container` must support
  /// construction from a range specified as a pair of input iterators.
  template <typename Container>
  Container get() const {
    google::cloud::internal::Base64Decoder decoder(base64_rep_);
    return Container(decoder.begin(), decoder.end());
  }

  /// @name Relational operators
  ///@{
  friend bool operator==(Bytes const& a, Bytes const& b) {
    return a.base64_rep_ == b.base64_rep_;
  }
  friend bool operator!=(Bytes const& a, Bytes const& b) { return !(a == b); }
  ///@}

  /**
   * Outputs string representation of the Bytes to the provided stream.
   *
   * @warning This is intended for debugging and human consumption only, not
   *     machine consumption, as the output format may change without notice.
   */
  friend std::ostream& operator<<(std::ostream& os, Bytes const& bytes);

 private:
  friend struct spanner_internal::SPANNER_CLIENT_NS::BytesInternals;

  std::string base64_rep_;  // valid base64 representation
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
StatusOr<spanner::Bytes> BytesFromBase64(std::string input);
std::string BytesToBase64(spanner::Bytes b);
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BYTES_H
