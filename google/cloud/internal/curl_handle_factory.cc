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

#include "google/cloud/internal/curl_handle_factory.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#ifndef _WIN32
struct BIOPtrCleanup {
  int operator()(BIO* b) const { return BIO_free(b); }
};

using BioPtr = std::unique_ptr<BIO, BIOPtrCleanup>;

struct X509InfoPtrCleanup {
  void operator()(STACK_OF(X509_INFO) * i) const {
    return sk_X509_INFO_pop_free(i, X509_INFO_free);
  }
};

using X509InfoPtr = std::unique_ptr<STACK_OF(X509_INFO), X509InfoPtrCleanup>;

#endif

Status SetCurlCAInMemory(CurlHandleFactory const& factory, SSL_CTX* ssl_ctx) {
#if _WIN32
  return internal::InternalError(
      "SSL callback function currently not supported in windows",
      GCP_ERROR_INFO());
#else
  X509_STORE* cert_store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!cert_store) {
    return internal::InternalError("SSL_CTX_get_cert_store returned NULL",
                                   GCP_ERROR_INFO());
  }

  // Add each of the provided certs to the store.
  for (auto const& cert : factory.ca_certs()) {
    BioPtr buf{BIO_new_mem_buf(cert.data(), static_cast<int>(cert.length()))};
    if (!buf) {
      return internal::InternalError("BIO_new_mem_buf returned NULL",
                                     GCP_ERROR_INFO());
    }
    X509InfoPtr info{
        PEM_X509_INFO_read_bio(buf.get(), nullptr, nullptr, nullptr)};
    if (!info) {
      return internal::InternalError("PEM_X509_INFO_read_bio returned NULL",
                                     GCP_ERROR_INFO());
    }

    for (decltype(sk_X509_INFO_num(info.get())) i = 0;
         i < sk_X509_INFO_num(info.get()); ++i) {
      X509_INFO* value = sk_X509_INFO_value(info.get(), i);
      if (value->x509) {
        X509_STORE_add_cert(cert_store, value->x509);
      }
      if (value->crl) {
        X509_STORE_add_crl(cert_store, value->crl);
      }
    }
  }

  return {};
#endif
}

}  // namespace

extern "C" {

static CURLcode SslCtxFunction(  // NOLINT(misc-use-anonymous-namespace)
    CURL*, void* ssl_ctx, void* userdata) {
  auto* handle_factory = reinterpret_cast<CurlHandleFactory*>(userdata);
  auto result = SetCurlCAInMemory(*handle_factory, (SSL_CTX*)ssl_ctx);
  if (!result.ok()) {
    GCP_LOG(ERROR) << result << "\n";
    return CURLE_ABORTED_BY_CALLBACK;
  }
  return CURLE_OK;
}
}

void CurlHandleFactory::SetCurlStringOption(CURL* handle, CURLoption option_tag,
                                            char const* value) {
  curl_easy_setopt(handle, option_tag, value);
}

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory() {
  static auto const* const kFactory =
      new auto(std::make_shared<DefaultCurlHandleFactory>());
  return *kFactory;
}

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory(
    Options const& options) {
  if (!options.get<CARootsFilePathOption>().empty() ||
      !options.get<experimental::CAInMemoryOption>().empty()) {
    return std::make_shared<DefaultCurlHandleFactory>(options);
  }

  return GetDefaultCurlHandleFactory();
}

DefaultCurlHandleFactory::DefaultCurlHandleFactory(Options const& o) {
  if (o.has<experimental::CAInMemoryOption>()) {
    ca_certs_ = o.get<experimental::CAInMemoryOption>();
    if (ca_certs_.empty()) {
      GCP_LOG(FATAL) << internal::InvalidArgumentError(
          "No CA certificates specified", GCP_ERROR_INFO());
    }
  } else {
    if (o.has<CARootsFilePathOption>()) {
      cainfo_ = o.get<CARootsFilePathOption>();
    }
    if (o.has<CAPathOption>()) capath_ = o.get<CAPathOption>();
  }
}

CurlPtr DefaultCurlHandleFactory::CreateHandle() {
  auto curl = MakeCurlPtr();
  SetCurlOptions(curl.get());
  return curl;
}

void DefaultCurlHandleFactory::CleanupHandle(CurlPtr h, HandleDisposition) {
  if (!h) return;
  char* ip;
  auto res = curl_easy_getinfo(h.get(), CURLINFO_LOCAL_IP, &ip);
  if (res == CURLE_OK && ip != nullptr) {
    std::lock_guard<std::mutex> lk(mu_);
    last_client_ip_address_ = ip;
  }
  h.reset();
}

CurlMulti DefaultCurlHandleFactory::CreateMultiHandle() {
  return CurlMulti(curl_multi_init());
}

void DefaultCurlHandleFactory::CleanupMultiHandle(CurlMulti m,
                                                  HandleDisposition) {
  m.reset();
}

void DefaultCurlHandleFactory::SetCurlOptions(CURL* handle) {
  if (!ca_certs_.empty()) {
    SetCurlStringOption(handle, CURLOPT_CAINFO, nullptr);
    SetCurlStringOption(handle, CURLOPT_CAPATH, nullptr);
    auto result = curl_easy_setopt(handle, CURLOPT_SSL_CTX_DATA, this);
    if (result != CURLE_OK) {
      GCP_LOG(FATAL) << internal::InternalError(curl_easy_strerror(result),
                                                GCP_ERROR_INFO());
    }
    result =
        curl_easy_setopt(handle, CURLOPT_SSL_CTX_FUNCTION, &SslCtxFunction);
    if (result != CURLE_OK) {
      GCP_LOG(FATAL) << internal::InternalError(curl_easy_strerror(result),
                                                GCP_ERROR_INFO());
    }
  } else {
    if (cainfo_) {
      SetCurlStringOption(handle, CURLOPT_CAINFO, cainfo_->c_str());
    }
    if (capath_) {
      SetCurlStringOption(handle, CURLOPT_CAPATH, capath_->c_str());
    }
  }
}

PooledCurlHandleFactory::PooledCurlHandleFactory(std::size_t maximum_size,
                                                 Options const& o)
    : maximum_size_(maximum_size),
      cainfo_(CAInfo(o)),
      capath_(CAPath(o)),
      ca_certs_(CACerts(o)) {}

PooledCurlHandleFactory::~PooledCurlHandleFactory() = default;

CurlPtr PooledCurlHandleFactory::CreateHandle() {
  std::unique_lock<std::mutex> lk(handles_mu_);
  if (!handles_.empty()) {
    auto handle = std::move(handles_.back());
    handles_.pop_back();
    lk.unlock();
    // Clear all the options in the handle, so we do not leak its previous
    // state.
    curl_easy_reset(handle.get());
    SetCurlOptions(handle.get());
    return handle;
  }
  ++active_handles_;
  lk.unlock();
  auto curl = MakeCurlPtr();
  SetCurlOptions(curl.get());
  return curl;
}

void PooledCurlHandleFactory::CleanupHandle(CurlPtr h, HandleDisposition d) {
  if (!h) return;
  // Querying the local IP can be expensive, as it may require a DNS lookup.
  // We should not perform such operations while holding a lock.
  char* ip;
  auto res = curl_easy_getinfo(h.get(), CURLINFO_LOCAL_IP, &ip);
  if (res == CURLE_OK && ip != nullptr) {
    std::unique_lock<std::mutex> lk(last_client_ip_address_mu_);
    last_client_ip_address_ = ip;
  }
  // Use a temporary data structure to release any excess handles *after* the
  // lock is released.
  std::vector<CurlPtr> released;
  std::unique_lock<std::mutex> lk(handles_mu_);
  if (d == HandleDisposition::kDiscard) {
    --active_handles_;
    return;
  }
  // If needed, release several handles to make room, amortizing the cost when
  // many threads are releasing handles at the same time.
  if (handles_.size() >= maximum_size_) {
    // Sometimes the application may be using a lot more handles than
    // `maximum_size_`. For example, if many threads demand a handle for
    // downloads, then each thread will have a handle.
    // When these handles are returned we want to minimize the locking overhead
    // (and contention) by removing them in larger blocks. At the same time, we
    // do not want to empty the pool because other threads may need some handles
    // from the pool.  Finally, when the number of active handles is close to
    // the maximum size of the pool, we just want to remove enough handles to
    // make room.
    //
    // Note that active_handles_ >= handles_.size() is a class invariant, and we
    // just checked that handles_.size() >= maximum_size_ > maximum_size / 2
    auto const release_count = (std::min)(handles_.size() - maximum_size_ / 2,
                                          active_handles_ - maximum_size_);
    released.reserve(release_count);
    auto const end = std::next(handles_.begin(), release_count);
    std::move(handles_.begin(), end, std::back_inserter(released));
    handles_.erase(handles_.begin(), end);
  }
  handles_.push_back(std::move(h));
  active_handles_ -= released.size();
}

CurlMulti PooledCurlHandleFactory::CreateMultiHandle() {
  std::unique_lock<std::mutex> lk(multi_handles_mu_);
  if (!multi_handles_.empty()) {
    auto m = std::move(multi_handles_.back());
    multi_handles_.pop_back();
    lk.unlock();
    return m;
  }
  ++active_multi_handles_;
  lk.unlock();
  return CurlMulti(curl_multi_init());
}

void PooledCurlHandleFactory::CleanupMultiHandle(CurlMulti m,
                                                 HandleDisposition d) {
  if (!m) return;
  // Use a temporary data structure to release any excess handles *after* the
  // lock is released.
  std::vector<CurlMulti> released;
  std::unique_lock<std::mutex> lk(multi_handles_mu_);
  if (d == HandleDisposition::kDiscard) {
    --active_multi_handles_;
    return;
  }
  // If needed, release several handles to make room, amortizing the cost when
  // many threads are releasing handles at the same time.
  if (multi_handles_.size() >= maximum_size_) {
    // Same idea as is CleanupHandle()
    auto const release_count =
        (std::min)(multi_handles_.size() - maximum_size_ / 2,
                   active_multi_handles_ - maximum_size_);
    released.reserve(release_count);
    auto const end = std::next(multi_handles_.begin(), release_count);
    std::move(multi_handles_.begin(), end, std::back_inserter(released));
    multi_handles_.erase(multi_handles_.begin(), end);
  }
  multi_handles_.push_back(std::move(m));
  active_multi_handles_ -= released.size();
}

void PooledCurlHandleFactory::SetCurlOptions(CURL* handle) {
  if (!ca_certs_.empty()) {
    SetCurlStringOption(handle, CURLOPT_CAINFO, nullptr);
    SetCurlStringOption(handle, CURLOPT_CAPATH, nullptr);
    auto result = curl_easy_setopt(handle, CURLOPT_SSL_CTX_DATA, this);
    if (result != CURLE_OK) {
      GCP_LOG(FATAL) << internal::InternalError(curl_easy_strerror(result),
                                                GCP_ERROR_INFO());
    }
    result =
        curl_easy_setopt(handle, CURLOPT_SSL_CTX_FUNCTION, &SslCtxFunction);
    if (result != CURLE_OK) {
      GCP_LOG(FATAL) << internal::InternalError(curl_easy_strerror(result),
                                                GCP_ERROR_INFO());
    }
  } else {
    if (cainfo_) {
      SetCurlStringOption(handle, CURLOPT_CAINFO, cainfo_->c_str());
    }
    if (capath_) {
      SetCurlStringOption(handle, CURLOPT_CAPATH, capath_->c_str());
    }
  }
}

absl::optional<std::string> PooledCurlHandleFactory::CAInfo(Options const& o) {
  if (!o.has<CARootsFilePathOption>()) return absl::nullopt;
  return o.get<CARootsFilePathOption>();
}

absl::optional<std::string> PooledCurlHandleFactory::CAPath(Options const& o) {
  if (!o.has<CAPathOption>()) return absl::nullopt;
  return o.get<CAPathOption>();
}

std::vector<absl::string_view> PooledCurlHandleFactory::CACerts(
    Options const& o) {
  if (!o.has<experimental::CAInMemoryOption>()) return {};
  if (o.get<experimental::CAInMemoryOption>().empty()) {
    GCP_LOG(FATAL) << internal::InvalidArgumentError(
        "No CA certificates specified", GCP_ERROR_INFO());
  }
  return o.get<experimental::CAInMemoryOption>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
