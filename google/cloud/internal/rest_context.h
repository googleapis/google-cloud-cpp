// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This type is used to track state and for decorators to modify across a
 * request and response round trip rpc.
 */
class RestContext {
 public:
  using HttpHeaders = std::unordered_map<std::string, std::vector<std::string>>;
  RestContext() = default;
  explicit RestContext(Options options, HttpHeaders headers)
      : options_(std::move(options)), headers_(std::move(headers)) {}
  explicit RestContext(Options options) : RestContext(std::move(options), {}) {}
  explicit RestContext(HttpHeaders headers)
      : RestContext({}, std::move(headers)) {}

  Options const& options() const { return options_; }

  HttpHeaders const& headers() const { return headers_; }

  // Adding a header/value pair that already exists results in the new value
  // appended to the list of values for the existing header.
  RestContext& AddHeader(std::string header, std::string value) &;
  RestContext&& AddHeader(std::string header, std::string value) && {
    return std::move(AddHeader(std::move(header), std::move(value)));
  }
  RestContext& AddHeader(std::pair<std::string, std::string> header) &;
  RestContext&& AddHeader(std::pair<std::string, std::string> header) && {
    return std::move(AddHeader(std::move(header)));
  }

  // Vector is empty if header name is not found.
  // Header names are case-insensitive; header values are case-sensitive.
  std::vector<std::string> GetHeader(std::string header) const;

  absl::optional<std::string> local_ip_address() const {
    return local_ip_address_;
  }
  void reset_local_ip_address() { local_ip_address_.reset(); }
  void set_local_ip_address(std::string a) { local_ip_address_ = std::move(a); }

  absl::optional<std::int32_t> local_port() const { return local_port_; }
  void reset_local_port() { local_port_.reset(); }
  void set_local_port(std::int32_t p) { local_port_ = p; }

  absl::optional<std::string> primary_ip_address() const {
    return primary_ip_address_;
  }
  void reset_primary_ip_address() { primary_ip_address_.reset(); }
  void set_primary_ip_address(std::string a) {
    primary_ip_address_ = std::move(a);
  }

  absl::optional<std::int32_t> primary_port() const { return primary_port_; }
  void reset_primary_port() { primary_port_.reset(); }
  void set_primary_port(std::int32_t p) { primary_port_ = p; }

  // The time spent in DNS lookups
  absl::optional<std::chrono::microseconds> namelookup_time() const {
    return namelookup_time_;
  }
  void reset_namelookup_time() { namelookup_time_.reset(); }
  void set_namelookup_time(std::chrono::microseconds us) {
    namelookup_time_ = us;
  }

  // The time spent setting the TCP/IP connection.
  absl::optional<std::chrono::microseconds> connect_time() const {
    return connect_time_;
  }
  void reset_connect_time() { connect_time_.reset(); }
  void set_connect_time(std::chrono::microseconds us) { connect_time_ = us; }

  // The time spent in the SSL handshake.
  absl::optional<std::chrono::microseconds> appconnect_time() const {
    return appconnect_time_;
  }
  void reset_appconnect_time() { appconnect_time_.reset(); }
  void set_appconnect_time(std::chrono::microseconds us) {
    appconnect_time_ = us;
  }

 private:
  friend bool operator==(RestContext const& lhs, RestContext const& rhs);
  Options options_;
  HttpHeaders headers_;
  absl::optional<std::string> local_ip_address_;
  absl::optional<std::int32_t> local_port_;
  absl::optional<std::string> primary_ip_address_;
  absl::optional<std::int32_t> primary_port_;
  absl::optional<std::chrono::microseconds> namelookup_time_;
  absl::optional<std::chrono::microseconds> connect_time_;
  absl::optional<std::chrono::microseconds> appconnect_time_;
};

bool operator==(RestContext const& lhs, RestContext const& rhs);
bool operator!=(RestContext const& lhs, RestContext const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H
