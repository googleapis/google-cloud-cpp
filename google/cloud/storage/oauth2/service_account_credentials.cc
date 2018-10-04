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

#include "google/cloud/storage/oauth2/service_account_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

ServiceAccountCredentialsInfo ParseServiceAccountCredentials(
    std::string const& content, std::string const& source,
    std::string const& default_token_uri) {
  namespace nl = storage::internal::nl;
  nl::json credentials = nl::json::parse(content, nullptr, false);
  if (credentials.is_discarded()) {
    google::cloud::internal::RaiseInvalidArgument(
        "Invalid ServiceAccountCredentials,"
        " parsing failed on data loaded from " +
        source);
  }
  char const PRIVATE_KEY_ID_KEY[] = "private_key_id";
  char const PRIVATE_KEY_KEY[] = "private_key";
  char const TOKEN_URI_KEY[] = "token_uri";
  char const CLIENT_EMAIL_KEY[] = "client_email";
  for (auto const& key :
       {PRIVATE_KEY_ID_KEY, PRIVATE_KEY_KEY, CLIENT_EMAIL_KEY}) {
    if (credentials.count(key) == 0U) {
      google::cloud::internal::RaiseInvalidArgument(
          "Invalid ServiceAccountCredentials, the " + std::string(key) +
          " field is missing on data loaded from " + source);
    }
    if (credentials.value(key, "").empty()) {
      google::cloud::internal::RaiseInvalidArgument(
          "Invalid ServiceAccountCredentials, the " + std::string(key) +
          " field is empty on data loaded from " + source);
    }
  }
  // The token_uri field may be missing, but may not be empty:
  if (credentials.count(TOKEN_URI_KEY) != 0U and
      credentials.value(TOKEN_URI_KEY, "").empty()) {
    google::cloud::internal::RaiseInvalidArgument(
        "Invalid ServiceAccountCredentials, the " + std::string(TOKEN_URI_KEY) +
        " field is empty on data loaded from " + source);
  }
  return ServiceAccountCredentialsInfo{
      credentials.value(PRIVATE_KEY_ID_KEY, ""),
      credentials.value(PRIVATE_KEY_KEY, ""),
      // Some credential formats (e.g. gcloud's ADC file) don't contain a
      // "token_uri" attribute in the JSON object.  In this case, we try using
      // the default value.
      credentials.value(TOKEN_URI_KEY, default_token_uri),
      credentials.value(CLIENT_EMAIL_KEY, ""),
  };
}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
