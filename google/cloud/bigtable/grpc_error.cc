// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/grpc_error.h"
#include <sstream>

namespace {
constexpr char const* KNOWN_STATUS_CODES[] = {
    "OK",
    "CANCELLED",
    "UNKNOWN",
    "INVALID_ARGUMENT",
    "DEADLINE_EXCEEDED",
    "NOT_FOUND",
    "ALREADY_EXISTS",
    "PERMISSION_DENIED",
    "RESOURCE_EXHAUSTED",
    "FAILED_PRECONDITION",
    "ABORTED",
    "OUT_OF_RANGE",
    "UNIMPLEMENTED",
    "INTERNAL",
    "UNAVAILABLE",
    "DATA_LOSS",
    "UNAUTHENTICATED",
};
constexpr int KNOWN_STATUS_CODES_SIZE =
    sizeof(KNOWN_STATUS_CODES) / sizeof(KNOWN_STATUS_CODES[0]);
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
GRpcError::GRpcError(char const* what, grpc::Status const& status)
    : std::runtime_error(CreateWhatString(what, status)),
      error_code_(status.error_code()),
      error_message_(status.error_message()),
      error_details_(status.error_details()) {}

std::string GRpcError::CreateWhatString(char const* what,
                                        grpc::Status const& status) {
  std::ostringstream os;
  os << what << ": " << status.error_message() << " [" << status.error_code()
     << "=";
  int const index = status.error_code();
  if (0 <= index && index < KNOWN_STATUS_CODES_SIZE) {
    os << KNOWN_STATUS_CODES[status.error_code()];
  } else {
    os << "(UNKNOWN CODE)";
  }
  os << "] - " << status.error_details();
  return os.str();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
