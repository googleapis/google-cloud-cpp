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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
CorsEntry ParseCors(internal::nl::json const& json) {
  auto parse_string_list = [](internal::nl::json const& json,
                              char const* field_name) {
    std::vector<std::string> list;
    if (json.count(field_name) != 0) {
      for (auto const& kv : json[field_name].items()) {
        list.emplace_back(kv.value().get<std::string>());
      }
    }
    return list;
  };
  CorsEntry result;
  if (json.count("maxAgeSeconds") != 0) {
    result.max_age_seconds = internal::ParseLongField(json, "maxAgeSeconds");
  }
  result.method = parse_string_list(json, "method");
  result.origin = parse_string_list(json, "origin");
  result.response_header = parse_string_list(json, "responseHeader");
  return result;
};

}  // namespace

std::ostream& operator<<(std::ostream& os, CorsEntry const& rhs) {
  auto join = [](char const* sep, std::vector<std::string> const& list) {
    if (list.empty()) {
      return std::string{};
    }
    return std::accumulate(++list.begin(), list.end(), list.front(),
                           [sep](std::string a, std::string const& b) {
                             a += sep;
                             a += b;
                             return a;
                           });
  };
  os << "CorsEntry={";
  char const* sep = "";
  if (rhs.max_age_seconds.has_value()) {
    os << "max_age_seconds=" << *rhs.max_age_seconds;
    sep = ", ";
  }
  return os << sep << "method=[" << join(", ", rhs.method) << "], origin=["
            << join(", ", rhs.origin) << "], response_header=["
            << join(", ", rhs.response_header) << "]}";
}

std::ostream& operator<<(std::ostream& os, BucketLogging const& rhs) {
  return os << "BucketLogging={log_bucket=" << rhs.log_bucket
            << ", log_prefix=" << rhs.log_prefix << "}";
}

BucketMetadata BucketMetadata::ParseFromJson(internal::nl::json const& json) {
  BucketMetadata result{};
  static_cast<CommonMetadata<BucketMetadata>&>(result) =
      CommonMetadata<BucketMetadata>::ParseFromJson(json);

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      result.acl_.emplace_back(BucketAccessControl::ParseFromJson(kv.value()));
    }
  }
  if (json.count("billing") != 0) {
    auto billing = json["billing"];
    BucketBilling b;
    b.requester_pays = internal::ParseBoolField(billing, "requesterPays");
    result.billing_ = b;
  }
  if (json.count("cors") != 0) {
    for (auto const& kv : json["cors"].items()) {
      result.cors_.emplace_back(ParseCors(kv.value()));
    }
  }
  if (json.count("defaultObjectAcl") != 0) {
    for (auto const& kv : json["defaultObjectAcl"].items()) {
      result.default_acl_.emplace_back(
          ObjectAccessControl::ParseFromJson(kv.value()));
    }
  }
  if (json.count("encryption") != 0) {
    BucketEncryption e;
    e.default_kms_key_name = json["encryption"].value("defaultKmsKeyName", "");
    result.encryption_ = std::move(e);
  }

  if (json.count("lifecycle") != 0) {
    auto lifecycle = json["lifecycle"];
    BucketLifecycle value;
    if (lifecycle.count("rule") != 0) {
      for (auto const& kv : lifecycle["rule"].items()) {
        value.rule.emplace_back(LifecycleRule::ParseFromJson(kv.value()));
      }
    }
    result.lifecycle_ = std::move(value);
  }

  result.location_ = json.value("location", "");

  if (json.count("logging") != 0) {
    auto logging = json["logging"];
    BucketLogging l;
    l.log_bucket = logging.value("logBucket", "");
    l.log_prefix = logging.value("logPrefix", "");
    result.logging_ = std::move(l);
  }
  result.project_number_ = internal::ParseLongField(json, "projectNumber");
  if (json.count("labels") > 0) {
    for (auto const& kv : json["labels"].items()) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }

  if (json.count("versioning") != 0) {
    auto versioning = json["versioning"];
    if (versioning.count("enabled") != 0) {
      BucketVersioning v{internal::ParseBoolField(versioning, "enabled")};
      result.versioning_ = v;
    }
  }

  if (json.count("website") != 0) {
    auto website = json["website"];
    BucketWebsite w;
    w.main_page_suffix = website.value("mainPageSuffix", "");
    w.not_found_page = website.value("notFoundPage", "");
    result.website_ = std::move(w);
  }
  return result;
}

BucketMetadata BucketMetadata::ParseFromString(std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload);
  return ParseFromJson(json);
}

bool BucketMetadata::operator==(BucketMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata<BucketMetadata> const&>(*this) ==
             rhs and
         acl_ == rhs.acl_ and billing_ == rhs.billing_ and
         cors_ == rhs.cors_ and default_acl_ == rhs.default_acl_ and
         encryption_ == rhs.encryption_ and
         project_number_ == rhs.project_number_ and
         lifecycle_ == rhs.lifecycle_ and location_ == rhs.location_ and
         logging_ == rhs.logging_ and labels_ == rhs.labels_ and
         versioning_ == rhs.versioning_ and website_ == rhs.website_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  os << "BucketMetadata={name=" << rhs.name();

  os << ", acl=[";
  char const* sep = "";
  for (auto const& acl : rhs.acl()) {
    os << sep << acl;
    sep = ", ";
  }
  os << "]";

  if (rhs.has_billing()) {
    auto previous_flags = os.flags();
    os << ", billing.requesterPays=" << std::boolalpha
       << rhs.billing().requester_pays;
    os.flags(previous_flags);
  }

  os << ", cors=[";
  sep = "";
  for (auto const& cors : rhs.cors()) {
    os << sep << cors;
    sep = ", ";
  }
  os << "]";

  os << ", default_acl=[";
  sep = "";
  for (auto const& acl : rhs.default_acl()) {
    os << sep << acl;
    sep = ", ";
  }
  os << "]";

  if (rhs.has_encryption()) {
    os << ", encryption.default_kms_key_name="
       << rhs.encryption().default_kms_key_name;
  }

  os << ", etag=" << rhs.etag() << ", id=" << rhs.id()
     << ", kind=" << rhs.kind();

  for (auto const& kv : rhs.labels_) {
    os << ", labels." << kv.first << "=" << kv.second;
  }

  if (rhs.has_lifecycle()) {
    os << ", lifecycle.rule=[";
    sep = "";
    for (auto const& r : rhs.lifecycle().rule) {
      os << sep << r;
      sep = ", ";
    }
    os << "]";
  }

  os << ", location=" << rhs.location();

  if (rhs.has_logging()) {
    os << ", logging=" << rhs.logging();
  }

  os << ", metageneration=" << rhs.metageneration() << ", name=" << rhs.name();

  if (rhs.has_owner()) {
    os << ", owner.entity=" << rhs.owner().entity
       << ", owner.entity_id=" << rhs.owner().entity_id;
  }

  os << ", project_number=" << rhs.project_number()
     << ", self_link=" << rhs.self_link()
     << ", storage_class=" << rhs.storage_class()
     << ", time_created=" << rhs.time_created().time_since_epoch().count()
     << ", updated=" << rhs.updated().time_since_epoch().count();

  if (rhs.versioning().has_value()) {
    auto previous_flags = os.flags();
    os << ", versioning.enabled=" << std::boolalpha
       << rhs.versioning()->enabled;
    os.flags(previous_flags);
  }

  if (rhs.has_website()) {
    os << ", website.main_page_suffix=" << rhs.website().main_page_suffix
       << ", website.not_found_page=" << rhs.website().not_found_page;
  }

  return os << "}";
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
