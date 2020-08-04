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

#include "google/cloud/storage/internal/hmac_key_requests.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  HmacKeyMetadata result{};
  result.access_id_ = json.value("accessId", "");
  result.etag_ = json.value("etag", "");
  result.id_ = json.value("id", "");
  result.kind_ = json.value("kind", "");
  result.project_id_ = json.value("projectId", "");
  result.service_account_email_ = json.value("serviceAccountEmail", "");
  result.state_ = json.value("state", "");
  if (json.count("timeCreated") != 0) {
    result.time_created_ =
        google::cloud::internal::ParseRfc3339(json.value("timeCreated", ""));
  }
  if (json.count("updated") != 0) {
    result.updated_ =
        google::cloud::internal::ParseRfc3339(json.value("updated", ""));
  }
  return result;
}

StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

std::ostream& operator<<(std::ostream& os, CreateHmacKeyRequest const& r) {
  os << "CreateHmacKeyRequest={project_id=" << r.project_id()
     << ", service_account=" << r.service_account();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<CreateHmacKeyResponse> CreateHmacKeyResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  CreateHmacKeyResponse result;
  result.kind = json.value("kind", "");
  result.secret = json.value("secret", "");
  if (json.count("metadata") != 0) {
    auto resource = HmacKeyMetadataParser::FromJson(json["metadata"]);
    if (!resource) {
      return std::move(resource).status();
    }
    result.metadata = *std::move(resource);
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, CreateHmacKeyResponse const& r) {
  return os << "CreateHmacKeyResponse={metadata=" << r.metadata
            << ", secret=[censored]"
            << "}";
}

std::ostream& operator<<(std::ostream& os, ListHmacKeysRequest const& r) {
  os << "ListHmacKeysRequest={project_id=" << r.project_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListHmacKeysResponse> ListHmacKeysResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }

  ListHmacKeysResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    auto parsed = internal::HmacKeyMetadataParser::FromJson(kv.value());
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListHmacKeysResponse const& r) {
  os << "ListHmacKeysResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<HmacKeyMetadata>(os, ", "));
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, DeleteHmacKeyRequest const& r) {
  os << "DeleteHmacKeyRequest={project_id=" << r.project_id()
     << ", access_id=" << r.access_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, GetHmacKeyRequest const& r) {
  os << "GetHmacKeyRequest={project_id=" << r.project_id()
     << ", access_id=" << r.access_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateHmacKeyRequest const& r) {
  os << "UpdateHmacKeyRequest={project_id=" << r.project_id()
     << ", access_id=" << r.access_id() << ", resource=" << r.resource();
  r.DumpOptions(os, ", ");
  return os << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
