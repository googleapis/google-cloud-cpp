// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/bytes.h"
#include <array>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Prints the bytes in the form B"...", where printable bytes are output
// normally, double quotes are backslash escaped, and non-printable characters
// are printed as a 3-digit octal escape sequence.
std::ostream& operator<<(std::ostream& os, Bytes const& bytes) {
  os << R"(B")";
  for (auto const byte : bytes.bytes_) {
    if (byte == '"') {
      os << R"(\")";
    } else if (std::isprint(byte)) {
      os << byte;
    } else {
      // This uses snprintf rather than iomanip so we don't mess up the
      // formatting on `os` for other streaming operations.
      std::array<char, sizeof(R"(\000)")> buf;
      auto n = std::snprintf(buf.data(), buf.size(), R"(\%03o)",
                             static_cast<unsigned char>(byte));
      if (n == static_cast<int>(buf.size() - 1)) {
        os << buf.data();
      } else {
        os << R"(\?)";
      }
    }
  }
  // Can't use raw string literal here because of a doxygen bug.
  return os << "\"";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct BytesInternals {
  static bigtable::Bytes Create(std::string rep) {
    bigtable::Bytes bytes;
    bytes.bytes_ = std::move(rep);
    return bytes;
  }

  static std::string GetRep(bigtable::Bytes&& bytes) {
    return std::move(bytes.bytes_);
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
