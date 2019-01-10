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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H_

#include "google/cloud/storage/internal/complex_option.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define the expiration time for a signed URL.
 */
struct ExpirationTime
    : public internal::ComplexOption<ExpirationTime,
                                     std::chrono::system_clock::time_point> {
  using ComplexOption<ExpirationTime,
                      std::chrono::system_clock::time_point>::ComplexOption;
  static char const* name() { return "expiration_time"; }
};

/**
 * Add a extension header to a signed URL.
 */
struct AddExtensionHeaderOption
    : public internal::ComplexOption<AddExtensionHeaderOption,
                                     std::pair<std::string, std::string>> {
  using ComplexOption<AddExtensionHeaderOption,
                      std::pair<std::string, std::string>>::ComplexOption;
  static char const* name() { return "expiration_time"; }
};

inline AddExtensionHeaderOption AddExtensionHeader(std::string header,
                                                   std::string value) {
  std::transform(
      header.begin(), header.end(), header.begin(),
      [](char c) -> char { return static_cast<char>(std::tolower(c)); });
  return AddExtensionHeaderOption(
      std::make_pair(std::move(header), std::move(value)));
}

/**
 * Add a extension header to a signed URL.
 */
struct AddQueryParameterOption
    : public internal::ComplexOption<AddQueryParameterOption, std::string> {
  using ComplexOption<AddQueryParameterOption, std::string>::ComplexOption;
  static char const* name() { return "query-parameter"; }

  /**
   * Escapes a string using URL encoding.
   *
   * This function acts as a compilation barrier because including
   * internal/curl_wrappers.h in this header would expose that header (and the
   * headers it depends on) in the public interface, and we want to avoid that.
   */
  static std::string UrlEscape(std::string const& value);
};

inline AddQueryParameterOption WithAcl() {
  return AddQueryParameterOption("acl");
}

inline AddQueryParameterOption WithBilling() {
  return AddQueryParameterOption("billing");
}

inline AddQueryParameterOption WithCompose() {
  return AddQueryParameterOption("compose");
}

inline AddQueryParameterOption WithCors() {
  return AddQueryParameterOption("cors");
}

inline AddQueryParameterOption WithEncryption() {
  return AddQueryParameterOption("encryption");
}

inline AddQueryParameterOption WithEncryptionConfig() {
  return AddQueryParameterOption("encryptionConfig");
}

inline AddQueryParameterOption WithGeneration(std::uint64_t generation) {
  return AddQueryParameterOption(
      "generation=" +
      AddQueryParameterOption::UrlEscape(std::to_string(generation)));
}

inline AddQueryParameterOption WithGenerationMarker(std::uint64_t generation) {
  return AddQueryParameterOption(
      "generation-marker=" +
      AddQueryParameterOption::UrlEscape(std::to_string(generation)));
}

inline AddQueryParameterOption WithLifecycle() {
  return AddQueryParameterOption("lifecycle");
}

inline AddQueryParameterOption WithLocation() {
  return AddQueryParameterOption("location");
}

inline AddQueryParameterOption WithLogging() {
  return AddQueryParameterOption("logging");
}

inline AddQueryParameterOption WithMarker(std::string const& marker) {
  return AddQueryParameterOption("marker=" +
                                 AddQueryParameterOption::UrlEscape(marker));
}

inline AddQueryParameterOption WithResponseContentDisposition(
    std::string const& disposition) {
  return AddQueryParameterOption(
      "response-content-disposition=" +
      AddQueryParameterOption::UrlEscape(disposition));
}

inline AddQueryParameterOption WithResponseContentType(
    std::string const& type) {
  return AddQueryParameterOption("response-content-type=" +
                                 AddQueryParameterOption::UrlEscape(type));
}

inline AddQueryParameterOption WithStorageClass() {
  return AddQueryParameterOption("storageClass");
}

inline AddQueryParameterOption WithTagging() {
  return AddQueryParameterOption("tagging");
}

inline AddQueryParameterOption WithUserProject(
    std::string const& user_project) {
  return AddQueryParameterOption(
      "userProject=" + AddQueryParameterOption::UrlEscape(user_project));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H_
