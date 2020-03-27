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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/format_time_point.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string PolicyDocumentRequest::StringToSign() const {
  using internal::nl::json;
  auto document = policy_document();

  json j;
  j["expiration"] = google::cloud::internal::FormatRfc3339(document.expiration);

  for (auto const& kv : document.conditions) {
    std::vector<std::string> elements = kv.elements();

    /**
     * If the elements is of size 2, we've encountered an exact match in
     * object form.  So we create a json object using the first element as the
     * key and the second element as the value.
     */
    if (elements.size() == 2) {
      json object;
      object[elements.at(0)] = elements.at(1);
      j["conditions"].push_back(object);
    } else {
      if (elements.at(0) == "content-length-range") {
        j["conditions"].push_back({elements.at(0), std::stol(elements.at(1)),
                                   std::stol(elements.at(2))});
      }
    }
  }

  return std::move(j).dump();
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentRequest const& r) {
  return os << "PolicyDocumentRequest={" << r.StringToSign() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
