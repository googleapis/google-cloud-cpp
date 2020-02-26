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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H

#include "google/cloud/storage/hashing_options.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/signed_url_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>
#include <map>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// The common data for SignUrlRequests.
class SignUrlRequestCommon {
 public:
  SignUrlRequestCommon() = default;
  SignUrlRequestCommon(std::string verb, std::string bucket_name,
                       std::string object_name)
      : verb_(std::move(verb)),
        bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)) {}

  std::string const& verb() const { return verb_; }
  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& object_name() const { return object_name_; }
  std::string const& sub_resource() const { return sub_resource_; }
  std::map<std::string, std::string> const& extension_headers() const {
    return extension_headers_;
  }
  std::multimap<std::string, std::string> const& query_parameters() const {
    return query_parameters_;
  }

  SigningAccount const& signing_account() const { return signing_account_; }
  SigningAccountDelegates const& signing_account_delegates() const {
    return signing_account_delegates_;
  }

  void SetOption(SubResourceOption const& o) {
    if (!o.has_value()) {
      return;
    }
    sub_resource_ = o.value();
  }

  void SetOption(AddExtensionHeaderOption const& o);

  void SetOption(AddQueryParameterOption const& o) {
    if (!o.has_value()) {
      return;
    }
    query_parameters_.insert(o.value());
  }

  void SetOption(SigningAccount const& o) { signing_account_ = o; }

  void SetOption(SigningAccountDelegates const& o) {
    signing_account_delegates_ = o;
  }

  /**
   * Splits the "object_name" by '/' delimiter
   *
   * "object_name" may contain '/' to represent the object path and these
   * '/' must not be escaped in "V4SignUrlRequest". This funtion helps in
   * splitting string of "object_name" in parts.
   */
  std::vector<std::string> ObjectNameParts() const;

 private:
  std::string verb_;
  std::string bucket_name_;
  std::string object_name_;
  std::string sub_resource_;
  std::map<std::string, std::string> extension_headers_;
  std::multimap<std::string, std::string> query_parameters_;

  SigningAccount signing_account_;
  SigningAccountDelegates signing_account_delegates_;
};

/**
 * Creates a V2 signed url.
 */
class V2SignUrlRequest {
 public:
  V2SignUrlRequest() = default;
  explicit V2SignUrlRequest(std::string verb, std::string bucket_name,
                            std::string object_name)
      : common_request_(std::move(verb), std::move(bucket_name),
                        std::move(object_name)),
        expiration_time_(DefaultExpirationTime()) {}

  std::string const& verb() const { return common_request_.verb(); }
  std::string const& bucket_name() const {
    return common_request_.bucket_name();
  }
  std::string const& object_name() const {
    return common_request_.object_name();
  }
  std::string const& sub_resource() const {
    return common_request_.sub_resource();
  }
  SigningAccount const& signing_account() const {
    return common_request_.signing_account();
  }
  SigningAccountDelegates const& signing_account_delegates() const {
    return common_request_.signing_account_delegates();
  }

  std::chrono::seconds expiration_time_as_seconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        expiration_time_.time_since_epoch());
  }

  /// Creates the string to be signed.
  std::string StringToSign() const;

  template <typename H, typename... T>
  V2SignUrlRequest& set_multiple_options(H&& h, T&&... tail) {
    SetOption(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  V2SignUrlRequest& set_multiple_options() { return *this; }

 private:
  static std::chrono::system_clock::time_point DefaultExpirationTime();

  void SetOption(MD5HashValue const& o) {
    if (!o.has_value()) {
      return;
    }
    md5_hash_value_ = o.value();
  }

  void SetOption(ContentType const& o) {
    if (!o.has_value()) {
      return;
    }
    content_type_ = o.value();
  }

  void SetOption(ExpirationTime const& o) {
    if (!o.has_value()) {
      return;
    }
    expiration_time_ = o.value();
  }

  void SetOption(SubResourceOption const& o) { common_request_.SetOption(o); }

  void SetOption(AddExtensionHeaderOption const& o) {
    common_request_.SetOption(o);
  }

  void SetOption(AddQueryParameterOption const& o) {
    common_request_.SetOption(o);
  }

  void SetOption(SigningAccount const& o) { common_request_.SetOption(o); }

  void SetOption(SigningAccountDelegates const& o) {
    common_request_.SetOption(o);
  }

  SignUrlRequestCommon common_request_;
  std::string md5_hash_value_;
  std::string content_type_;
  std::chrono::system_clock::time_point expiration_time_;
};

std::ostream& operator<<(std::ostream& os, V2SignUrlRequest const& r);

/**
 * Creates a V4 signed url.
 */
class V4SignUrlRequest {
 public:
  V4SignUrlRequest() : expires_(0) {}
  explicit V4SignUrlRequest(std::string verb, std::string bucket_name,
                            std::string object_name)
      : common_request_(std::move(verb), std::move(bucket_name),
                        std::move(object_name)),
        timestamp_(DefaultTimestamp()),
        expires_(DefaultExpires()) {}

  std::string const& verb() const { return common_request_.verb(); }
  std::string const& bucket_name() const {
    return common_request_.bucket_name();
  }
  std::string const& object_name() const {
    return common_request_.object_name();
  }

  std::vector<std::string> ObjectNameParts() const;

  std::string const& sub_resource() const {
    return common_request_.sub_resource();
  }
  SigningAccount const& signing_account() const {
    return common_request_.signing_account();
  }
  SigningAccountDelegates const& signing_account_delegates() const {
    return common_request_.signing_account_delegates();
  }

  std::chrono::system_clock::time_point timestamp() const { return timestamp_; }
  std::chrono::seconds expires() const { return expires_; }

  /// Add any headers that the application developer did not provide.
  void AddMissingRequiredHeaders();

  /// Creates the query string with the required query parameters.
  std::string CanonicalQueryString(std::string const& client_id) const;

  /**
   * Creates the "canonical request" document.
   *
   * The "canonical request" is a string that encapsulates all the request
   * parameters (verb, resource, query parameters, headers) that will be part
   * of the signed document. This member function is mostly used for testing.
   */
  std::string CanonicalRequest(std::string const& client_id) const;

  /// Creates the V4 string to be signed.
  std::string StringToSign(std::string const& client_id) const;

  template <typename H, typename... T>
  V4SignUrlRequest& set_multiple_options(H&& h, T&&... tail) {
    SetOption(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  V4SignUrlRequest& set_multiple_options() { return *this; }

 private:
  static std::chrono::system_clock::time_point DefaultTimestamp();
  static std::chrono::seconds DefaultExpires();

  void SetOption(SignedUrlTimestamp const& o) {
    if (!o.has_value()) {
      return;
    }
    timestamp_ = o.value();
  }

  void SetOption(SignedUrlDuration const& o) {
    if (!o.has_value()) {
      return;
    }
    expires_ = o.value();
  }

  void SetOption(SubResourceOption const& o) { common_request_.SetOption(o); }

  void SetOption(AddExtensionHeaderOption const& o) {
    common_request_.SetOption(o);
  }

  void SetOption(AddQueryParameterOption const& o) {
    common_request_.SetOption(o);
  }

  void SetOption(SigningAccount const& o) { common_request_.SetOption(o); }

  void SetOption(SigningAccountDelegates const& o) {
    common_request_.SetOption(o);
  }

  std::string CanonicalRequestHash(std::string const& client_id) const;

  std::string Scope() const;

  std::multimap<std::string, std::string> CanonicalQueryParameters(
      std::string const& client_id) const;

  std::string SignedHeaders() const;

  SignUrlRequestCommon common_request_;
  std::chrono::system_clock::time_point timestamp_;
  std::chrono::seconds expires_;
};

std::ostream& operator<<(std::ostream& os, V4SignUrlRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGNED_URL_REQUESTS_H
