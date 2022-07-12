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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_FACTORY_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/curl_wrappers.h"
#include "google/cloud/options.h"
#include "absl/types/optional.h"
#include <deque>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Implements the Factory Pattern for CURL handles (and multi-handles).
 */
class CurlHandleFactory {
 public:
  virtual ~CurlHandleFactory() = default;

  virtual rest_internal::CurlPtr CreateHandle() = 0;
  virtual void CleanupHandle(rest_internal::CurlPtr) = 0;

  virtual rest_internal::CurlMulti CreateMultiHandle() = 0;
  virtual void CleanupMultiHandle(rest_internal::CurlMulti&&) = 0;

  virtual std::string LastClientIpAddress() const = 0;

  // For testing and debug only, we do not need anything more elaborate as this
  // class is in `internal::`.
  virtual absl::optional<std::string> cainfo() const = 0;
  virtual absl::optional<std::string> capath() const = 0;

 protected:
  // Only virtual for testing purposes.
  virtual void SetCurlStringOption(CURL* handle, CURLoption option_tag,
                                   char const* value);
};

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory(
    Options const& options);
std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory();

/**
 * Implements the default CurlHandleFactory.
 *
 * This implementation of the CurlHandleFactory does not save handles, it
 * creates a new handle on each call to `CreateHandle()` and releases the
 * handle on `CleanupHandle()`.
 */
class DefaultCurlHandleFactory : public CurlHandleFactory {
 public:
  DefaultCurlHandleFactory() = default;
  explicit DefaultCurlHandleFactory(Options const& o);

  rest_internal::CurlPtr CreateHandle() override;
  void CleanupHandle(rest_internal::CurlPtr) override;

  rest_internal::CurlMulti CreateMultiHandle() override;
  void CleanupMultiHandle(rest_internal::CurlMulti&&) override;

  std::string LastClientIpAddress() const override {
    std::lock_guard<std::mutex> lk(mu_);
    return last_client_ip_address_;
  }

  absl::optional<std::string> cainfo() const override { return cainfo_; }
  absl::optional<std::string> capath() const override { return capath_; }

 private:
  void SetCurlOptions(CURL* handle);

  mutable std::mutex mu_;
  std::string last_client_ip_address_;
  absl::optional<std::string> cainfo_;
  absl::optional<std::string> capath_;
};

/**
 * Implements a CurlHandleFactory that pools handles.
 *
 * This implementation keeps up to N handles in memory, they are only released
 * when the factory is destructed.
 */
class PooledCurlHandleFactory : public CurlHandleFactory {
 public:
  PooledCurlHandleFactory(std::size_t maximum_size, Options const& o);
  explicit PooledCurlHandleFactory(std::size_t maximum_size)
      : PooledCurlHandleFactory(maximum_size, {}) {}
  ~PooledCurlHandleFactory() override;

  rest_internal::CurlPtr CreateHandle() override;
  void CleanupHandle(rest_internal::CurlPtr) override;

  rest_internal::CurlMulti CreateMultiHandle() override;
  void CleanupMultiHandle(rest_internal::CurlMulti&&) override;

  std::string LastClientIpAddress() const override {
    std::lock_guard<std::mutex> lk(mu_);
    return last_client_ip_address_;
  }

  // Test only
  std::size_t CurrentHandleCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return handles_.size();
  }
  // Test only
  std::size_t CurrentMultiHandleCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return multi_handles_.size();
  }

  absl::optional<std::string> cainfo() const override { return cainfo_; }
  absl::optional<std::string> capath() const override { return capath_; }

 private:
  void SetCurlOptions(CURL* handle);

  std::size_t maximum_size_;
  mutable std::mutex mu_;
  std::deque<CURL*> handles_;
  std::deque<CURLM*> multi_handles_;
  std::string last_client_ip_address_;
  absl::optional<std::string> cainfo_;
  absl::optional<std::string> capath_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_HANDLE_FACTORY_H
