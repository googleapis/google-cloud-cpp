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
#include "absl/strings/numbers.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<bool> ParseBoolField(nlohmann::json const& json,
                              char const* field_name) {
  if (json.count(field_name) == 0) return false;
  auto const& f = json[field_name];
  if (f.is_boolean()) return f.get<bool>();

  if (f.is_string()) {
    auto v = f.get<std::string>();
    if (v == "true") return true;
    if (v == "false") return false;
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a boolean, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

StatusOr<std::int32_t> ParseIntField(nlohmann::json const& json,
                                     char const* field_name) {
  if (json.count(field_name) == 0) return 0;
  auto const& f = json[field_name];
  if (f.is_number()) return f.get<std::int32_t>();
  if (f.is_string()) {
    std::int32_t v;
    if (absl::SimpleAtoi(f.get_ref<std::string const&>(), &v)) return v;
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a std::int32_t, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

StatusOr<std::uint32_t> ParseUnsignedIntField(nlohmann::json const& json,
                                              char const* field_name) {
  if (json.count(field_name) == 0) return 0;
  auto const& f = json[field_name];
  if (f.is_number()) return f.get<std::uint32_t>();
  if (f.is_string()) {
    std::uint32_t v;
    if (absl::SimpleAtoi(f.get_ref<std::string const&>(), &v)) return v;
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a std::uint32_t, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

StatusOr<std::int64_t> ParseLongField(nlohmann::json const& json,
                                      char const* field_name) {
  if (json.count(field_name) == 0) return 0;
  auto const& f = json[field_name];
  if (f.is_number()) return f.get<std::int64_t>();
  if (f.is_string()) {
    std::int64_t v;
    if (absl::SimpleAtoi(f.get_ref<std::string const&>(), &v)) return v;
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a std::int64_t, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

StatusOr<std::uint64_t> ParseUnsignedLongField(nlohmann::json const& json,
                                               char const* field_name) {
  if (json.count(field_name) == 0) return 0;
  auto const& f = json[field_name];
  if (f.is_number()) return f.get<std::uint64_t>();
  if (f.is_string()) {
    std::uint64_t v;
    if (absl::SimpleAtoi(f.get_ref<std::string const&>(), &v)) return v;
  }
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a std::uint64_t, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

StatusOr<std::chrono::system_clock::time_point> ParseTimestampField(
    nlohmann::json const& json, char const* field_name) {
  if (json.count(field_name) == 0) {
    return std::chrono::system_clock::time_point{};
  }
  auto const& f = json[field_name];
  if (f.is_string()) return google::cloud::internal::ParseRfc3339(f);
  std::ostringstream os;
  os << "Error parsing field <" << field_name
     << "> as a timestamp, json=" << json;
  return Status(StatusCode::kInvalidArgument, std::move(os).str());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
