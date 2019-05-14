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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_

#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <curl/curl.h>
#include <functional>
#include <map>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// Hold a CURL* handle and automatically clean it up.
using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

/// Hold a CURLM* handle and automatically clean it up.
using CurlMulti = std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)>;

/// Hold a character string created by CURL use correct deleter.
using CurlString = std::unique_ptr<char, decltype(&curl_free)>;

using CurlHeaders = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

using CurlReceivedHeaders = std::multimap<std::string, std::string>;
std::size_t CurlAppendHeaderData(CurlReceivedHeaders& received_headers,
                                 char const* data, std::size_t size);

using CurlShare = std::unique_ptr<CURLSH, decltype(&curl_share_cleanup)>;

/// Returns true if the SSL locking callbacks are installed.
bool SslLockingCallbacksInstalled();

/// Initializes (if needed) the SSL locking callbacks.
void CurlInitializeOnce(ClientOptions const& options);

/// Returns the id of the SSL library used by libcurl.
std::string CurlSslLibraryId();

/// Determines if the SSL library requires locking.
bool SslLibraryNeedsLocking(std::string const& curl_ssl_id);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_WRAPPERS_H_
