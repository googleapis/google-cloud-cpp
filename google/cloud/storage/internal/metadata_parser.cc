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

#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/internal/throw_delegate.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
bool ParseBoolField(nlohmann::json const& json, char const* field_name) {
  if (json.count(field_name) == 0) {
    return false;
  }
  auto const& f = json[field_name];
  if (f.is_boolean()) {
    return f.get<bool>();
  }
  if (f.is_string()) {
    auto v = f.get<std::string>();
    if (v == "true") {
      return true;
    }
    if (v == "false") {
      return false;
    }
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a boolean, json=" << json;
  google::cloud::internal::ThrowInvalidArgument(os.str());
}

std::int32_t ParseIntField(nlohmann::json const& json, char const* field_name) {
  if (json.count(field_name) == 0) {
    return 0;
  }
  auto const& f = json[field_name];
  if (f.is_number()) {
    return f.get<std::int32_t>();
  }
  if (f.is_string()) {
    return std::stoi(f.get_ref<std::string const&>());
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as an std::int32_t, json=" << json;
  google::cloud::internal::ThrowInvalidArgument(os.str());
}

std::uint32_t ParseUnsignedIntField(nlohmann::json const& json,
                                    char const* field_name) {
  if (json.count(field_name) == 0) {
    return 0;
  }
  auto const& f = json[field_name];
  if (f.is_number()) {
    return f.get<std::uint32_t>();
  }
  if (f.is_string()) {
    auto v = std::stoul(f.get_ref<std::string const&>());
    if (v <= (std::numeric_limits<std::uint32_t>::max)()) {
      return static_cast<std::uint32_t>(v);
    }
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as an std::uint32_t, json=" << json;
  google::cloud::internal::ThrowInvalidArgument(os.str());
}

std::int64_t ParseLongField(nlohmann::json const& json,
                            char const* field_name) {
  if (json.count(field_name) == 0) {
    return 0;
  }
  auto const& f = json[field_name];
  if (f.is_number()) {
    return f.get<std::int64_t>();
  }
  if (f.is_string()) {
    return std::stoll(f.get_ref<std::string const&>());
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as an std::int64_t, json=" << json;
  google::cloud::internal::ThrowInvalidArgument(os.str());
}

std::uint64_t ParseUnsignedLongField(nlohmann::json const& json,
                                     char const* field_name) {
  if (json.count(field_name) == 0) {
    return 0;
  }
  auto const& f = json[field_name];
  if (f.is_number()) {
    return f.get<std::uint64_t>();
  }
  if (f.is_string()) {
    return std::stoull(f.get_ref<std::string const&>());
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as an std::uint64_t, json=" << json;
  google::cloud::internal::ThrowInvalidArgument(os.str());
}

std::chrono::system_clock::time_point ParseTimestampField(
    nlohmann::json const& json, char const* field_name) {
  if (json.count(field_name) == 0) {
    return std::chrono::system_clock::time_point{};
  }
  return google::cloud::internal::ParseRfc3339(json[field_name]);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
