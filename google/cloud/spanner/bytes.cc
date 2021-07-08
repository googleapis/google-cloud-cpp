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

#include "google/cloud/spanner/bytes.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/status.h"
#include <array>
#include <cctype>
#include <climits>
#include <cstdio>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

using ::google::cloud::internal::Base64Decoder;

// Prints the bytes in the form B"...", where printable bytes are output
// normally, double quotes are backslash escaped, and non-printable characters
// are printed as a 3-digit octal escape sequence.
std::ostream& operator<<(std::ostream& os, Bytes const& bytes) {
  os << R"(B")";
  for (auto const byte : Base64Decoder(bytes.base64_rep_)) {
    if (byte == '"') {
      os << R"(\")";
    } else if (std::isprint(byte)) {
      os << byte;
    } else {
      // This uses snprintf rather than iomanip so we don't mess up the
      // formatting on `os` for other streaming operations.
      std::array<char, sizeof(R"(\000)")> buf;
      auto n = std::snprintf(buf.data(), buf.size(), R"(\%03o)", byte);
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct BytesInternals {
  static spanner::Bytes Create(std::string rep) {
    spanner::Bytes bytes;
    bytes.base64_rep_ = std::move(rep);
    return bytes;
  }

  static std::string GetRep(spanner::Bytes&& bytes) {
    return std::move(bytes.base64_rep_);
  }
};

// Construction from a base64-encoded US-ASCII `std::string`.
StatusOr<spanner::Bytes> BytesFromBase64(std::string input) {
  auto status = google::cloud::internal::ValidateBase64String(input);
  if (!status.ok()) return status;
  return BytesInternals::Create(std::move(input));
}

// Conversion to a base64-encoded US-ASCII `std::string`.
std::string BytesToBase64(spanner::Bytes b) {
  return BytesInternals::GetRep(std::move(b));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
