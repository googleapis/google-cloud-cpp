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

#include "google/cloud/internal/curl_wrappers.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#if LIBRESSL_VERSION_NUMBER
// LibreSSL calls itself OpenSSL > 2.0, but it really is based on SSL 1.0.2
// and requires locks.
#define GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS 1
#elif OPENSSL_VERSION_NUMBER < 0x10100000L  // Older than version 1.1.0
// Before 1.1.0 OpenSSL requires locks to be used by multiple threads.
#define GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS 1
#else
#define GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS 0
#endif

#if GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS
std::vector<std::mutex> ssl_locks;

// A callback to lock and unlock the mutexes needed by the SSL library.
extern "C" void RestSslLockingCb(int mode, int type, char const*, int) {
  if ((mode & CRYPTO_LOCK) != 0) {
    ssl_locks[type].lock();
  } else {
    ssl_locks[type].unlock();
  }
}

void InitializeSslLocking(bool enable_ssl_callbacks) {
  std::string curl_ssl = CurlSslLibraryId();
  // Only enable the lock callbacks if needed. We need to look at what SSL
  // library is used by libcurl.  Many of them work fine without any additional
  // setup.
  if (!SslLibraryNeedsLocking(curl_ssl)) {
    GCP_LOG(INFO) << "SSL locking callbacks not installed because the"
                  << " SSL library does not need them.";
    return;
  }
  if (!enable_ssl_callbacks) {
    GCP_LOG(INFO) << "SSL locking callbacks not installed because the"
                  << " application disabled them.";
    return;
  }
  if (CRYPTO_get_locking_callback() != nullptr) {
    GCP_LOG(INFO) << "SSL locking callbacks not installed because there are"
                  << " callbacks already installed.";
    return;
  }
  // If we need to configure locking, make sure the library we linked against is
  // the same library that libcurl is using. In environments where both
  // OpenSSL/1.0.2 are OpenSSL/1.1.0 are available it is easy to link the wrong
  // one, and that does not work because they have completely different symbols,
  // despite the version numbers suggesting otherwise.
  std::string expected_prefix = curl_ssl;
  std::transform(expected_prefix.begin(), expected_prefix.end(),
                 expected_prefix.begin(),
                 [](char x) { return x == '/' ? ' ' : x; });
  // LibreSSL seems to be using semantic versioning, so just check the major
  // version.
  if (absl::StartsWith(expected_prefix, "LibreSSL 2")) {
    expected_prefix = "LibreSSL 2";
  }
#ifdef OPENSSL_VERSION
  std::string openssl_v = OpenSSL_version(OPENSSL_VERSION);
#else
  std::string openssl_v = SSLeay_version(SSLEAY_VERSION);
#endif  // OPENSSL_VERSION
  // We check the prefix for two reasons: (a) for some libraries it is enough
  // that the major version matches (e.g. LibreSSL), and (b) because the
  // `openssl_v` string sometimes reads `OpenSSL 1.1.0 May 2018` while the
  // string reported by libcurl would be `OpenSSL/1.1.0`, sigh...
  if (!absl::StartsWith(openssl_v, expected_prefix)) {
    std::ostringstream os;
    os << "Mismatched versions of OpenSSL linked in libcurl vs. the version"
       << " linked by the Google Cloud Storage C++ library.\n"
       << "libcurl is linked against " << curl_ssl
       << "\nwhile the google cloud storage library links against " << openssl_v
       << "\nMismatched versions are not supported.  The Google Cloud Storage"
       << "\nC++ library needs to configure the OpenSSL library used by libcurl"
       << "\nand this is not possible if you link different versions.";
    // This is a case where printing to stderr is justified, this happens during
    // the library initialization, nothing else may get reported to the
    // application developer.
    std::cerr << os.str() << "\n";
    google::cloud::internal::ThrowRuntimeError(os.str());
  }

  // If we get to this point, we need to initialize the OpenSSL library to have
  // a callback, the documentation:
  //     https://www.openssl.org/docs/man1.0.2/crypto/threads.html
  // is a bit hard to parse, but basically one must create CRYPTO_num_lock()
  // mutexes, and a single callback for all of them.
  //
  GCP_LOG(INFO) << "Installing SSL locking callbacks.";
  ssl_locks =
      std::vector<std::mutex>(static_cast<std::size_t>(CRYPTO_num_locks()));
  CRYPTO_set_locking_callback(RestSslLockingCb);

  // The documentation also recommends calling CRYPTO_THREADID_set_callback() to
  // setup a function to return thread ids as integers (or pointers). Writing a
  // portable function like that would be non-trivial, C++ thread identifiers
  // are opaque, they cannot be converted to integers, pointers or the native
  // thread type.
  //
  // Fortunately the documentation also states that a default version is
  // provided:
  //    "If the application does not register such a callback using
  //     CRYPTO_THREADID_set_callback(), then a default implementation
  //     is used"
  // then goes on to describe how this default version works:
  //    "on Windows and BeOS this uses the system's default thread identifying
  //     APIs, and on all other platforms it uses the address of errno."
  // Luckily, the C++11 standard guarantees that `errno` is a thread-specific
  // object:
  //     https://en.cppreference.com/w/cpp/error/errno
  // There are no guarantees (as far as I know) that the errno used by a
  // C-library like OpenSSL is the same errno as the one used by C++. But such
  // an implementation would be terribly broken: it would be impossible to call
  // C functions from C++. In my (coryan@google.com) opinion, we can rely on the
  // default version.
}
#else
void InitializeSslLocking(bool) {}
#endif  // GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS

void InitializeSigPipeHandler(bool enable_sigpipe_handler) {
  if (!enable_sigpipe_handler) {
    return;
  }
#if defined(SIGPIPE)
  std::signal(SIGPIPE, SIG_IGN);
#endif  // SIGPIPE
}

/// Automatically initialize (and cleanup) the libcurl library.
class CurlInitializer {
 public:
  CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }
  ~CurlInitializer() { curl_global_cleanup(); }
};

std::size_t constexpr kMaxDebugLength = 128;

std::string CleanupDebugData(char const* data, std::size_t size) {
  auto const n = (std::min)(size, kMaxDebugLength);
  auto text = std::string{data, n};
  std::transform(text.begin(), text.end(), text.begin(),
                 [](auto c) { return std::isprint(c) ? c : '.'; });
  return text;
}

}  // namespace

std::string CurlSslLibraryId() {
  auto* vinfo = curl_version_info(CURLVERSION_NOW);
  auto const is_null = vinfo == nullptr || vinfo->ssl_version == nullptr;
  return is_null ? "" : vinfo->ssl_version;
}

bool SslLibraryNeedsLocking(std::string const& curl_ssl_id) {
  // Based on:
  //    https://curl.haxx.se/libcurl/c/threadsafe.html
  // Only these library prefixes require special configuration for using safely
  // with multiple threads.
  return (absl::StartsWith(curl_ssl_id, "OpenSSL/1.0") ||
          absl::StartsWith(curl_ssl_id, "LibreSSL/2"));
}

long VersionToCurlCode(std::string const& v) {  // NOLINT(google-runtime-int)
  if (v == "1.0") return CURL_HTTP_VERSION_1_0;
  if (v == "1.1") return CURL_HTTP_VERSION_1_1;
#if CURL_AT_LEAST_VERSION(7, 33, 0)
  // CURL_HTTP_VERSION_2_0 and CURL_HTTP_VERSION_2 are aliases.
  if (v == "2.0" || v == "2") return CURL_HTTP_VERSION_2_0;
#endif  // CURL >= 7.33.0
#if CURL_AT_LEAST_VERSION(7, 47, 0)
  if (v == "2TLS") return CURL_HTTP_VERSION_2TLS;
#endif  // CURL >= 7.47.0
#if CURL_AT_LEAST_VERSION(7, 66, 0)
  // google-cloud-cpp requires curl >= 7.47.0. All the previous codes exist at
  // that version, but the next one is more recent.
  if (v == "3") return CURL_HTTP_VERSION_3;
#endif  // CURL >= 7.66.0
  return CURL_HTTP_VERSION_NONE;
}

bool SslLockingCallbacksInstalled() {
#if GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS
  return !ssl_locks.empty();
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS
}

CurlPtr MakeCurlPtr() {
  auto handle = CurlPtr(curl_easy_init(), &curl_easy_cleanup);
  // We get better performance using a slightly larger buffer (128KiB) than the
  // default buffer size set by libcurl (16KiB).  We ignore errors because
  // failing to set this parameter just affects performance by a small amount.
  (void)curl_easy_setopt(handle.get(), CURLOPT_BUFFERSIZE, 128 * 1024L);
  return handle;
}

std::size_t CurlAppendHeaderData(CurlReceivedHeaders& received_headers,
                                 char const* data, std::size_t size) {
  if (size <= 2) {
    // Empty header (including the \r\n), ignore.
    return size;
  }
  if ('\r' != data[size - 2] || '\n' != data[size - 1]) {
    // Invalid header (should end in \r\n), ignore.
    return size;
  }
  auto const* separator = std::find(data, data + size, ':');
  std::string header_name = std::string(data, separator);
  std::string header_value;
  // If there is a value, capture it, but ignore the final \r\n.
  if (static_cast<std::size_t>(separator - data) < size - 2) {
    header_value = std::string(separator + 2, data + size - 2);
  }
  std::transform(header_name.begin(), header_name.end(), header_name.begin(),
                 [](char x) { return std::tolower(x); });
  received_headers.emplace(std::move(header_name), std::move(header_value));
  return size;
}

std::string DebugInfo(char const* data, std::size_t size) {
  return absl::StrCat("== curl(Info): ", absl::string_view{data, size});
}

std::string DebugRecvHeader(char const* data, std::size_t size) {
  return absl::StrCat("<< curl(Recv Header): ", absl::string_view{data, size});
}

std::string DebugSendHeader(char const* data, std::size_t size) {
  // libcurl delivers multiple headers in a single payload, separated by '\n'.
  auto const payload = absl::string_view{data, size};
  // We want to truncate the portion of the payload following this ": Bearer" to
  // at most 32 characters, skipping everything else until any newline.
  auto const bearer = absl::string_view{": Bearer "};
  auto const limit = bearer.size() + 32;
  auto const bearer_pos = payload.find(bearer);
  if (bearer_pos != std::string::npos) {
    auto const nl_pos = payload.find('\n', bearer_pos);
    auto const prefix = payload.substr(0, bearer_pos);
    auto trailer = absl::string_view{};
    auto body = payload.substr(bearer_pos);
    if (nl_pos != std::string::npos) {
      trailer = payload.substr(nl_pos);
      body = payload.substr(bearer_pos, nl_pos - bearer_pos);
    }
    auto marker = body.size() > limit ? "...<truncated>..." : "";
    body = absl::ClippedSubstr(std::move(body), 0, limit);
    return absl::StrCat(">> curl(Send Header): ", prefix, body, marker,
                        trailer);
  }
  return absl::StrCat(">> curl(Send Header): ", payload);
}

std::string DebugInData(char const* data, std::size_t size) {
  return absl::StrCat("<< curl(Recv Data): size=", size,
                      " data=", CleanupDebugData(data, size), "\n");
}

std::string DebugOutData(char const* data, std::size_t size) {
  return absl::StrCat(">> curl(Send Data): size=", size,
                      " data=", CleanupDebugData(data, size), "\n");
}

void CurlInitializeOnce(Options const& options) {
  static CurlInitializer curl_initializer;
  static bool const kInitialized = [&options]() {
    // The Google Cloud Storage C++ client library depends on libcurl, which
    // can use different SSL libraries. Depending on the SSL implementation,
    // we need to take action to be thread-safe. More details can be found here:
    //
    //     https://curl.haxx.se/libcurl/c/threadsafe.html
    //
    InitializeSslLocking(options.get<EnableCurlSslLockingOption>());

    // libcurl recommends turning on `CURLOPT_NOSIGNAL` for threaded
    // applications: "Note that setting `CURLOPT_NOSIGNAL` to 0L will not work
    // in a threaded situation as there will be race where libcurl risks
    // restoring the former signal handler while another thread should still
    // ignore it."
    //
    // libcurl further recommends that we set up our own signal handler for
    // SIGPIPE when using multiple threads: "When `CURLOPT_NOSIGNAL` is set to
    // 1L, your application needs to deal with the risk of a `SIGPIPE` (that at
    // least the OpenSSL backend can trigger)".
    //
    //     https://curl.haxx.se/libcurl/c/threadsafe.html
    //
    InitializeSigPipeHandler(options.get<EnableCurlSigpipeHandlerOption>());
    return true;
  }();
  static_cast<void>(kInitialized);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
