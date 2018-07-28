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
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include <iostream>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r) {
  os << "ReadObjectRangeRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", begin=" << r.begin()
     << ", end=" << r.end();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ReadObjectRangeResponse ReadObjectRangeResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto loc = response.headers.find(std::string("content-range"));
  if (response.headers.end() == loc) {
    google::cloud::internal::RaiseInvalidArgument(
        "invalid http response for ReadObjectRange");
  }

  std::string const& content_range_value = loc->second;
  auto function = __func__;  // capture this function name, not the lambda's
  auto raise_error = [&content_range_value, &function]() {
    std::ostringstream os;
    os << static_cast<char const*>(function)
       << " invalid format for content-range header <" << content_range_value
       << ">";
    google::cloud::internal::RaiseInvalidArgument(os.str());
  };
  char unit_descriptor[] = "bytes";
  if (0 != content_range_value.find(unit_descriptor)) {
    raise_error();
  }
  char const* buffer = content_range_value.data();
  auto size = content_range_value.size();
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

std::ostream& operator<<(std::ostream& os, ReadObjectRangeResponse const& r) {
  return os << "ReadObjectRangeResponse={range=" << r.first_byte << "-"
            << r.last_byte << "/" << r.object_size << ", contents=\n"
            << BinaryDataAsDebugString(r.contents.data(), r.contents.size())
            << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
