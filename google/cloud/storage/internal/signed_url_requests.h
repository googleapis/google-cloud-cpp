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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H_

#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/signed_url_options.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>
#include <map>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Requests the Google Cloud Storage service account for a project.
 */
class SignUrlRequest {
 public:
  SignUrlRequest() = default;
  explicit SignUrlRequest(std::string verb, std::string bucket_name,
                          std::string object_name);

  std::string const& verb() const { return verb_; }
  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& object_name() const { return object_name_; }
  std::chrono::seconds expiration_time_as_seconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        expiration_time_.time_since_epoch());
  }

  /// Creates the blob to be signed.
  std::string StringToSign() const;

  template <typename H, typename... T>
  SignUrlRequest& set_multiple_options(H&& h, T&&... tail) {
    set_option(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  SignUrlRequest& set_multiple_options() { return *this; }

 private:
  void set_option(MD5HashValue const& o) {
    if (!o.has_value()) {
      return;
    }
    md5_hash_value_ = o.value();
  }

  void set_option(ContentType const& o) {
    if (!o.has_value()) {
      return;
    }
    content_type_ = o.value();
  }

  void set_option(ExpirationTime const& o) {
    if (!o.has_value()) {
      return;
    }
    expiration_time_ = o.value();
  }

  void set_option(AddExtensionHeaderOption const& o) {
    if (!o.has_value()) {
      return;
    }
    auto res = extension_headers_.insert(o.value());
    if (!res.second) {
      // The element already exists, we need to append:
      res.first->second.push_back(',');
      res.first->second.append(o.value().second);
    }
  }

  void set_option(AddQueryParameterOption const& o) {
    if (!o.has_value()) {
      return;
    }
    query_parameters_.push_back(o.value());
  }

  std::string verb_;
  std::string bucket_name_;
  std::string object_name_;
  std::string md5_hash_value_;
  std::string content_type_;
  std::chrono::system_clock::time_point expiration_time_;
  std::map<std::string, std::string> extension_headers_;
  std::vector<std::string> query_parameters_;
};

std::ostream& operator<<(std::ostream& os, SignUrlRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H_
