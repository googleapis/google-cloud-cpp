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
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/format_time_point.h"
#include <iomanip>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

internal::nl::json TransformConditions(
    std::vector<PolicyDocumentCondition> const& conditions) {
  CurlHandle curl;
  auto res = internal::nl::json::array();
  for (auto const& kv : conditions) {
    std::vector<std::string> elements = kv.elements();
    /**
     * If the elements is of size 2, we've encountered an exact match in
     * object form.  So we create a json object using the first element as the
     * key and the second element as the value.
     */
    if (elements.size() == 2) {
      internal::nl::json object;
      object[elements.at(0)] = elements.at(1);
      res.emplace_back(std::move(object));
    } else {
      if (elements.at(0) == "content-length-range") {
        res.push_back({elements.at(0), std::stol(elements.at(1)),
                       std::stol(elements.at(2))});
      } else {
        res.push_back({elements.at(0), elements.at(1), elements.at(2)});
      }
    }
  }
  return res;
}
}  // namespace

std::string PolicyDocumentRequest::StringToSign() const {
  using internal::nl::json;
  auto const document = policy_document();

  json j;
  j["expiration"] = google::cloud::internal::FormatRfc3339(document.expiration);
  j["conditions"] = TransformConditions(document.conditions);

  return std::move(j).dump();
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentRequest const& r) {
  return os << "PolicyDocumentRequest={" << r.StringToSign() << "}";
}

void PolicyDocumentV4Request::SetOption(AddExtensionFieldOption const& o) {
  if (!o.has_value()) {
    return;
  }
  extension_fields_.emplace_back(
      std::make_pair(std::move(o.value().first), std::move(o.value().second)));
}

void PolicyDocumentV4Request::SetOption(PredefinedAcl const& o) {
  if (!o.has_value()) {
    return;
  }
  extension_fields_.emplace_back(std::make_pair("acl", o.HeaderName()));
}

void PolicyDocumentV4Request::SetOption(BucketBoundHostname const& o) {
  if (!o.has_value()) {
    bucket_bound_domain_.reset();
    return;
  }
  bucket_bound_domain_ = o.value();
}

void PolicyDocumentV4Request::SetOption(Scheme const& o) {
  if (!o.has_value()) {
    return;
  }
  scheme_ = o.value();
}

void PolicyDocumentV4Request::SetOption(VirtualHostname const& o) {
  virtual_host_name_ = o.has_value() && o.value();
}

std::chrono::system_clock::time_point PolicyDocumentV4Request::ExpirationDate()
    const {
  return document_.timestamp + document_.expiration;
}

std::string PolicyDocumentV4Request::Url() const {
  if (bucket_bound_domain_) {
    return scheme_ + "://" + *bucket_bound_domain_ + "/";
  }
  if (virtual_host_name_) {
    return scheme_ + "://" + policy_document().bucket +
           ".storage.googleapis.com/";
  }
  return scheme_ + "://storage.googleapis.com/" + policy_document().bucket +
         "/";
}

std::string PolicyDocumentV4Request::Credentials() const {
  return signing_email_ + "/" +
         google::cloud::internal::FormatV4SignedUrlScope(document_.timestamp) +
         "/auto/storage/goog4_request";
}

std::string PolicyDocumentV4Request::StringToSign() const {
  using internal::nl::json;
  auto const document = policy_document();

  json j;

  std::vector<PolicyDocumentCondition> conditions;
  for (auto const& field : extension_fields_) {
    conditions.push_back(PolicyDocumentCondition({field.first, field.second}));
  }
  std::copy(document.conditions.begin(), document.conditions.end(),
            std::back_inserter(conditions));
  conditions.push_back(PolicyDocumentCondition({"key", document.object}));
  conditions.push_back(PolicyDocumentCondition(
      {"x-goog-date", google::cloud::internal::FormatV4SignedUrlTimestamp(
                          document_.timestamp)}));
  conditions.push_back(
      PolicyDocumentCondition({"x-goog-credential", Credentials()}));
  conditions.push_back(
      PolicyDocumentCondition({"x-goog-algorithm", "GOOG4-RSA-SHA256"}));
  j["conditions"] = TransformConditions(conditions);
  j["expiration"] = google::cloud::internal::FormatRfc3339(ExpirationDate());
  return std::move(j).dump();
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentV4Request const& r) {
  return os << "PolicyDocumentRequest={" << r.StringToSign() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
