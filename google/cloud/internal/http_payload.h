// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_PAYLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_PAYLOAD_H

#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Allows the payload and trailers of an HTTP response to be read.
class HttpPayload {
 public:
  static constexpr std::size_t kDefaultReadSize = 1024 * 1024;
  virtual ~HttpPayload() = default;

  // Always reads up to N bytes (as specified in buffer) from the payload and
  // write tp the provided buffer. Read can be called multiple times in order to
  // read the entire payload.
  // Returns number of bytes actually read into buffer from the payload.
  virtual StatusOr<std::size_t> Read(absl::Span<char> buffer) = 0;
};

// This function makes one or more HttpPayload::Read calls and writes all the
// bytes from the payload to a buffer it allocates.
StatusOr<std::string> ReadAll(
    std::unique_ptr<HttpPayload> payload,
    std::size_t read_size = HttpPayload::kDefaultReadSize);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_PAYLOAD_H
