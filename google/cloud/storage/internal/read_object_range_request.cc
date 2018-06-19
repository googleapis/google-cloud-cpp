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

#include "google/cloud/storage/internal/read_object_range_request.h"
#include <sstream>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
ReadObjectRangeResponse ReadObjectRangeResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto loc = response.headers.find(std::string("content-range"));
  if (response.headers.end() == loc) {
    google::cloud::internal::RaiseInvalidArgument(
        "invalid http response for ReadObjectRange");
  }

  std::string const& header = loc->second;
  auto raise_error = [&header]() {
    std::ostringstream os;
    os << static_cast<char const*>(__func__)
       << " invalid format for content-range header <" << header << ">";
    google::cloud::internal::RaiseInvalidArgument(os.str());
  };
  char unit_descriptor[] = "bytes";
  if (0 != header.find(unit_descriptor)) {
    raise_error();
  }
  char const* buffer = header.data();
  auto size = header.size();
  // skip the initial "bytes " string.
  buffer += sizeof(unit_descriptor);

  if (size < 2) {
    raise_error();
  }

  if (buffer[0] == '*' and buffer[1] == '/') {
    // The header is just the indication of size ('bytes */<size>'), parse that.
    buffer += 2;
    long long object_size;
    auto count = std::sscanf(buffer, "%lld", &object_size);
    if (count != 1) {
      raise_error();
    }
    return ReadObjectRangeResponse{std::move(response.payload), 0, 0,
                                   object_size};
  }

  long long first_byte;
  long long last_byte;
  long long object_size;
  auto count = std::sscanf(buffer, "%lld-%lld/%lld", &first_byte, &last_byte,
                           &object_size);
  if (count != 3) {
    raise_error();
  }

  return ReadObjectRangeResponse{std::move(response.payload), first_byte,
                                 last_byte, object_size};
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
