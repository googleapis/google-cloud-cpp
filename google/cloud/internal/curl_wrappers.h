// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRAPPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRAPPERS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <curl/curl.h>
#include <map>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifndef CURL_AT_LEAST_VERSION
#define CURL_AT_LEAST_VERSION(Ma, Mi, Pa) \
  (LIBCURL_VERSION_NUM >= ((((Ma) << 16) | ((Mi) << 8)) | (Pa)))
#endif  // CURL_AT_LEAST_VERSION

/// Hold a CURL* handle and automatically clean it up.
using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

/// Create a new (wrapped) CURL* with one-time configuration options set.
CurlPtr MakeCurlPtr();

/// Hold a CURLM* handle and automatically clean it up.
using CurlMulti = std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)>;

/// Hold a character string created by CURL use correct deleter.
using CurlString = std::unique_ptr<char, decltype(&curl_free)>;

using CurlHeaders = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

using CurlReceivedHeaders = std::multimap<std::string, std::string>;
std::size_t CurlAppendHeaderData(CurlReceivedHeaders& received_headers,
                                 char const* data, std::size_t size);

std::string DebugInfo(char const* data, std::size_t size);
std::string DebugRecvHeader(char const* data, std::size_t size);
std::string DebugSendHeader(char const* data, std::size_t size);
std::string DebugInData(char const* data, std::size_t size);
std::string DebugOutData(char const* data, std::size_t size);

using CurlShare = std::unique_ptr<CURLSH, decltype(&curl_share_cleanup)>;

/// Returns true if the SSL locking callbacks are installed.
bool SslLockingCallbacksInstalled();

/// Return the default global options
Options CurlInitializeOptions(Options options);

/// Initializes (if needed) the SSL locking callbacks.
void CurlInitializeOnce(Options const& options);

/// Returns the id of the SSL library used by libcurl.
std::string CurlSslLibraryId();

/// Determines if the SSL library requires locking.
bool SslLibraryNeedsLocking(std::string const& curl_ssl_id);

/// Convert a HTTP version string to the CURL codes
long VersionToCurlCode(std::string const& v);  // NOLINT(google-runtime-int)

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_WRAPPERS_H
