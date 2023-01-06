// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/rest_parse_json_error.h"
#include "google/cloud/internal/rest_response.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

StatusCode MapHttpCodeToStatus(long code) {  // NOLINT(google-runtime-int)
  if (code < 0 || code > 600) return StatusCode::kUnknown;
  return google::cloud::rest_internal::MapHttpCodeToStatus(
      static_cast<std::int32_t>(code));
}

}  // namespace

Status AsStatus(HttpResponse const& http_response) {
  auto const code = MapHttpCodeToStatus(http_response.status_code);
  if (code == StatusCode::kOk) return Status{};

  auto p = rest_internal::ParseJsonError(
      static_cast<int>(http_response.status_code), http_response.payload);
  return Status(code, std::move(p.first), std::move(p.second));
}

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs) {
  os << "status_code=" << rhs.status_code << ", headers={";
  os << absl::StrJoin(rhs.headers, ", ", absl::PairFormatter(": "));
  return os << "}, payload=<" << rhs.payload << ">";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
