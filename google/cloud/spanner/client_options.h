// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <cstdint>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The configuration parameters for spanner connections.
 */
class ConnectionOptions {
 public:
  /// The default options, using `grpc::GoogleDefaultCredentials()`.
  ConnectionOptions();

  /// Default parameters, using an explicit credentials object.
  explicit ConnectionOptions(std::shared_ptr<grpc::ChannelCredentials> c);

  ConnectionOptions& set_credentials(
      std::shared_ptr<grpc::ChannelCredentials> v) {
    credentials_ = std::move(v);
    return *this;
  }
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  ConnectionOptions& set_endpoint(std::string v) {
    endpoint_ = std::move(v);
    return *this;
  }
  std::string const& endpoint() const { return endpoint_; }

  bool clog_enabled() const { return clog_enabled_; }
  ConnectionOptions& enable_clog() {
    clog_enabled_ = true;
    return *this;
  }
  ConnectionOptions& disable_clog() {
    clog_enabled_ = false;
    return *this;
  }

  bool tracing_enabled(std::string const& component) const {
    return tracing_components_.find(component) != tracing_components_.end();
  }
  ConnectionOptions& enable_tracing(std::string const& component) {
    tracing_components_.insert(component);
    return *this;
  }
  ConnectionOptions& disable_tracing(std::string const& component) {
    tracing_components_.erase(component);
    return *this;
  }

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string endpoint_;
  bool clog_enabled_ = false;
  std::set<std::string> tracing_components_;
};

/// Options passed to `Client::Read` or `Client::PartitionRead`.
struct ReadOptions {
  /**
   * If non-empty, the name of an index on a database table. This index is used
   * instead of the table primary key when interpreting the `KeySet`and sorting
   * result rows.
   */
  std::string index_name;

  /**
   * Limit on the number of rows to yield, or 0 for no limit.
   * A limit cannot be specified when calling`PartitionRead`.
   */
  std::int64_t limit = 0;
};

inline bool operator==(ReadOptions const& lhs, ReadOptions const& rhs) {
  return lhs.limit == rhs.limit && lhs.index_name == rhs.index_name;
}

inline bool operator!=(ReadOptions const& lhs, ReadOptions const& rhs) {
  return !(lhs == rhs);
}

/// Options passed to `Client::PartitionRead` or `Client::PartitionQuery`
using PartitionOptions = google::spanner::v1::PartitionOptions;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_
