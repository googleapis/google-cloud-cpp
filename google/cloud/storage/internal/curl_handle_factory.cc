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

#include "google/cloud/storage/internal/curl_handle_factory.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::once_flag default_curl_handle_factory_initialized;
std::shared_ptr<CurlHandleFactory> default_curl_handle_factory;

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory() {
  std::call_once(default_curl_handle_factory_initialized, [] {
    default_curl_handle_factory = std::make_shared<DefaultCurlHandleFactory>();
  });
  return default_curl_handle_factory;
}

CurlPtr DefaultCurlHandleFactory::CreateHandle() {
  return CurlPtr(curl_easy_init(), &curl_easy_cleanup);
}

void DefaultCurlHandleFactory::CleanupHandle(CurlPtr&& h) { h.reset(); }

CurlMulti DefaultCurlHandleFactory::CreateMultiHandle() {
  return CurlMulti(curl_multi_init(), &curl_multi_cleanup);
}

void DefaultCurlHandleFactory::CleanupMultiHandle(CurlMulti&& m) { m.reset(); }

PooledCurlHandleFactory::PooledCurlHandleFactory(std::size_t maximum_size)
    : maximum_size_(maximum_size) {
  handles_.reserve(maximum_size);
  multi_handles_.reserve(maximum_size);
}

PooledCurlHandleFactory::~PooledCurlHandleFactory() {
  for (auto* h : handles_) {
    curl_easy_cleanup(h);
  }
  for (auto* m : multi_handles_) {
    curl_multi_cleanup(m);
  }
}

CurlPtr PooledCurlHandleFactory::CreateHandle() {
  std::unique_lock<std::mutex> lk(mu_);
  if (not handles_.empty()) {
    CURL* handle = handles_.back();
    // Clear all the options in the handle so we do not leak its previous state.
    (void)curl_easy_reset(handle);
    handles_.pop_back();
    return CurlPtr(handle, &curl_easy_cleanup);
  }
  return CurlPtr(curl_easy_init(), &curl_easy_cleanup);
}

void PooledCurlHandleFactory::CleanupHandle(CurlPtr&& h) {
  std::unique_lock<std::mutex> lk(mu_);
  if (handles_.size() >= maximum_size_) {
    CURL* tmp = handles_.front();
    handles_.erase(handles_.begin());
    curl_easy_cleanup(tmp);
  }
  handles_.push_back(h.get());
  h.release();
}

CurlMulti PooledCurlHandleFactory::CreateMultiHandle() {
  std::unique_lock<std::mutex> lk(mu_);
  if (not multi_handles_.empty()) {
    CURL* m = multi_handles_.back();
    multi_handles_.pop_back();
    return CurlMulti(m, &curl_multi_cleanup);
  }
  return CurlMulti(curl_multi_init(), &curl_multi_cleanup);
}

void PooledCurlHandleFactory::CleanupMultiHandle(CurlMulti&& m) {
  std::unique_lock<std::mutex> lk(mu_);
  if (multi_handles_.size() >= maximum_size_) {
    CURLM* tmp = multi_handles_.front();
    multi_handles_.erase(multi_handles_.begin());
    curl_multi_cleanup(tmp);
  }
  multi_handles_.push_back(m.get());
  m.release();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
