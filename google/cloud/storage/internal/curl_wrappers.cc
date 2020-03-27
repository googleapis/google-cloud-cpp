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

#include "google/cloud/storage/internal/curl_wrappers.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/log.h"
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
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
// The Google Cloud Storage C++ client library depends on libcurl, which depends
// on many different SSL libraries, depending on the library the library needs
// to take action to be thread-safe. More details can be found here:
//
//     https://curl.haxx.se/libcurl/c/threadsafe.html
//
std::once_flag ssl_locking_initialized;

// libcurl recommends turning on CURLOPT_NOSIGNAL for multi-threaded
// applications: "Note that setting CURLOPT_NOSIGNAL to 0L will not work in a
// threaded situation as there will be race where libcurl risks restoring the
// former signal handler while another thread should still ignore it."
//
// libcurl further recommends that we setup our own signal handler for SIGPIPE
// when using multiple threads: "When CURLOPT_NOSIGNAL is set to 1L, your
// application needs to deal with the risk of a SIGPIPE (that at least the
// OpenSSL backend can trigger)".
//
//     https://curl.haxx.se/libcurl/c/threadsafe.html
//
std::once_flag sigpipe_handler_initialized;

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
extern "C" void SslLockingCb(int mode, int type, char const*, int) {
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
  if (expected_prefix.rfind("LibreSSL 2", 0) == 0) {
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
  if (openssl_v.rfind(expected_prefix, 0) != 0) {
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
  CRYPTO_set_locking_callback(SslLockingCb);

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
}  // namespace

std::string CurlSslLibraryId() {
  auto vinfo = curl_version_info(CURLVERSION_NOW);
  return vinfo->ssl_version;
}

bool SslLibraryNeedsLocking(std::string const& curl_ssl_id) {
  // Based on:
  //    https://curl.haxx.se/libcurl/c/threadsafe.html
  // Only these library prefixes require special configuration for using safely
  // with multiple threads.
  return (curl_ssl_id.rfind("OpenSSL/1.0", 0) == 0 ||
          curl_ssl_id.rfind("LibreSSL/2", 0) == 0);
}

bool SslLockingCallbacksInstalled() {
#if GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS
  return !ssl_locks.empty();
#else
  return false;
#endif  // GOOGLE_CLOUD_CPP_SSL_REQUIRES_LOCKS
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
  auto separator = std::find(data, data + size, ':');
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

void CurlInitializeOnce(ClientOptions const& options) {
  static CurlInitializer curl_initializer;
  std::call_once(ssl_locking_initialized, InitializeSslLocking,
                 options.enable_ssl_locking_callbacks());
  std::call_once(sigpipe_handler_initialized, InitializeSigPipeHandler,
                 options.enable_sigpipe_handler());
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
