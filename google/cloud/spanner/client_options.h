// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H

#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * ClientOptions allows the caller to set a variety of options when
 * constructing a `Client` instance.
 */
class ClientOptions {
 public:
  ClientOptions() = default;
  ClientOptions(ClientOptions const&) = default;
  ClientOptions& operator=(ClientOptions const&) = default;
  ClientOptions(ClientOptions&&) = default;
  ClientOptions& operator=(ClientOptions&&) = default;

  /**
   * Convert the `ClientOptions` to the new, recommended way to represent
   * options of all varieties, `google::cloud::Options`.
   */
  explicit operator Options() const { return Options(query_options_); }

  /// Returns the `QueryOptions`
  QueryOptions const& query_options() const { return query_options_; }

  /// Sets the `QueryOptions`
  ClientOptions& set_query_options(QueryOptions qo) {
    query_options_ = std::move(qo);
    return *this;
  }

  friend bool operator==(ClientOptions const& a, ClientOptions const& b) {
    return a.query_options_ == b.query_options_;
  }

  friend bool operator!=(ClientOptions const& a, ClientOptions const& b) {
    return !(a == b);
  }

 private:
  QueryOptions query_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H
