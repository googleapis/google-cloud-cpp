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

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

CurlHttpPayload::CurlHttpPayload(std::unique_ptr<CurlImpl> impl,
                                 Options options)
    : impl_(std::move(impl)), options_(std::move(options)) {}

StatusOr<std::size_t> CurlHttpPayload::Read(absl::Span<char> buffer) {
  return impl_->Read(buffer);
}

StatusOr<std::string> ReadAll(std::unique_ptr<HttpPayload> payload,
                              std::size_t read_size) {
  std::string output_buffer;
  // Allocate buf on the heap as large values of read_size could exceed stack
  // size.
  auto buf = absl::make_unique<char[]>(read_size);
  StatusOr<std::size_t> read_status;
  do {
    read_status = payload->Read({&buf[0], read_size});
    if (!read_status.ok()) return std::move(read_status).status();
    output_buffer.append(buf.get(), read_status.value());
  } while (read_status.value() > 0);
  return output_buffer;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
