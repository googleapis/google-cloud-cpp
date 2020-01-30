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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <chrono>
#include <string>
#include <vector>

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
  AddExtensionHeaderOption(std::string header, std::string value)
      : ComplexOption(std::make_pair(std::move(header), std::move(value))) {}
  static char const* name() { return "expiration_time"; }
};

inline AddExtensionHeaderOption AddExtensionHeader(std::string header,
                                                   std::string value) {
  return AddExtensionHeaderOption(std::move(header), std::move(value));
}

/**
 * Add a extension header to a signed URL.
 */
struct AddQueryParameterOption
    : public internal::ComplexOption<AddQueryParameterOption,
                                     std::pair<std::string, std::string>> {
  using ComplexOption<AddQueryParameterOption,
                      std::pair<std::string, std::string>>::ComplexOption;
  AddQueryParameterOption(std::string key, std::string value)
      : ComplexOption(std::make_pair(std::move(key), std::move(value))) {}
  AddQueryParameterOption(char const* key, std::string value)
      : ComplexOption(std::make_pair(std::string(key), std::move(value))) {}
  static char const* name() { return "query-parameter"; }
};

inline AddQueryParameterOption WithGeneration(std::uint64_t generation) {
  return AddQueryParameterOption("generation", std::to_string(generation));
}

inline AddQueryParameterOption WithGenerationMarker(std::uint64_t generation) {
  return AddQueryParameterOption("generation-marker",
                                 std::to_string(generation));
}

inline AddQueryParameterOption WithUserProject(std::string user_project) {
  return AddQueryParameterOption("userProject", std::move(user_project));
}

inline AddQueryParameterOption WithMarker(std::string marker) {
  return AddQueryParameterOption("marker", std::move(marker));
}

inline AddQueryParameterOption WithResponseContentDisposition(
    std::string disposition) {
  return AddQueryParameterOption("response-content-disposition",
                                 std::move(disposition));
}

inline AddQueryParameterOption WithResponseContentType(
    std::string const& type) {
  return AddQueryParameterOption("response-content-type", type);
}

/**
 * Specify a sub-resource in a signed URL.
 */
struct SubResourceOption
    : public internal::ComplexOption<SubResourceOption, std::string> {
  using ComplexOption<SubResourceOption, std::string>::ComplexOption;
  static char const* name() { return "query-parameter"; }
};

inline SubResourceOption WithAcl() { return SubResourceOption("acl"); }

inline SubResourceOption WithBilling() { return SubResourceOption("billing"); }

inline SubResourceOption WithCompose() { return SubResourceOption("compose"); }

inline SubResourceOption WithCors() { return SubResourceOption("cors"); }

inline SubResourceOption WithEncryption() {
  return SubResourceOption("encryption");
}

inline SubResourceOption WithEncryptionConfig() {
  return SubResourceOption("encryptionConfig");
}

inline SubResourceOption WithLifecycle() {
  return SubResourceOption("lifecycle");
}

inline SubResourceOption WithLocation() {
  return SubResourceOption("location");
}

inline SubResourceOption WithLogging() { return SubResourceOption("logging"); }

inline SubResourceOption WithStorageClass() {
  return SubResourceOption("storageClass");
}

inline SubResourceOption WithTagging() { return SubResourceOption("tagging"); }

/**
 * Define the timestamp duration for a V4 signed URL.
 */
struct SignedUrlTimestamp
    : public internal::ComplexOption<SignedUrlTimestamp,
                                     std::chrono::system_clock::time_point> {
  using ComplexOption<SignedUrlTimestamp,
                      std::chrono::system_clock::time_point>::ComplexOption;
  static char const* name() { return "x-good-date"; }
};

/**
 * Define the duration for a V4 signed URL.
 */
struct SignedUrlDuration
    : public internal::ComplexOption<SignedUrlDuration, std::chrono::seconds> {
  using ComplexOption<SignedUrlDuration, std::chrono::seconds>::ComplexOption;
  static char const* name() { return "x-goog-expires"; }
};

/**
 * Specify the service account used to sign a blob.
 *
 * With this option the application can sign a URL or policy document using a
 * different account than the account associated with the current credentials.
 */
struct SigningAccount
    : public internal::ComplexOption<SigningAccount, std::string> {
  using ComplexOption<SigningAccount, std::string>::ComplexOption;
  static char const* name() { return "signing-account"; }
};

/**
 * Specify the sequence of delegates used to sign a blob.
 *
 * With this option the application can sign a URL even if the account
 * associated with the current credentials does not have direct
 * `roles/iam.serviceAccountTokenCreator` on the target service account.
 */
struct SigningAccountDelegates
    : public internal::ComplexOption<SigningAccountDelegates,
                                     std::vector<std::string>> {
  using ComplexOption<SigningAccountDelegates,
                      std::vector<std::string>>::ComplexOption;
  static char const* name() { return "signing-account-delegates"; }
};
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SIGNED_URL_OPTIONS_H
