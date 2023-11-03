// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DIRECTED_READ_REPLICAS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DIRECTED_READ_REPLICAS_H

#include "google/cloud/spanner/version.h"
#include "absl/types/optional.h"
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Indicates the type of replica.
 */
enum class ReplicaType {
  kReadWrite,  // Read-write replicas support both reads and writes.
  kReadOnly,   // Read-only replicas only support reads (not writes).
};

/**
 * The directed-read replica selector.
 *
 * Callers must provide one or more of the following fields:
 *   - location: One of the regions within the multi-region configuration
 *     of your database.
 *   - type: The type of the replica.
 */
class ReplicaSelection {
 public:
  // Only replicas in the location and of the given type will be used
  // to process the request.
  ReplicaSelection(std::string location, ReplicaType type)
      : location_(std::move(location)), type_(type) {}

  // Replicas in the location, of any available type, will be used to
  // process the request.
  explicit ReplicaSelection(std::string location)
      : location_(std::move(location)), type_(absl::nullopt) {}

  // Replicas of the given type, in the nearest available location, will
  // be used to process the request.
  explicit ReplicaSelection(ReplicaType type)
      : location_(absl::nullopt), type_(type) {}

  absl::optional<std::string> const& location() const { return location_; }
  absl::optional<ReplicaType> const& type() const { return type_; }

 private:
  absl::optional<std::string> location_;
  absl::optional<ReplicaType> type_;
};

inline bool operator==(ReplicaSelection const& a, ReplicaSelection const& b) {
  return a.location() == b.location() && a.type() == b.type();
}

inline bool operator!=(ReplicaSelection const& a, ReplicaSelection const& b) {
  return !(a == b);
}

/**
 * An `IncludeReplicas` contains an ordered list of `ReplicaSelection`s
 * that should be considered when serving requests.
 *
 * When `auto_failover_disabled` is set, requests will NOT be routed to
 * a healthy replica outside the list when all replicas in the list are
 * unavailable/unhealthy.
 */
class IncludeReplicas {
 public:
  IncludeReplicas(std::initializer_list<ReplicaSelection> replica_selections,
                  bool auto_failover_disabled = false)
      : replica_selections_(replica_selections),
        auto_failover_disabled_(auto_failover_disabled) {}

  std::vector<ReplicaSelection> const& replica_selections() const {
    return replica_selections_;
  }
  bool auto_failover_disabled() const { return auto_failover_disabled_; }

 private:
  std::vector<ReplicaSelection> replica_selections_;
  bool auto_failover_disabled_;
};

/**
 * An `ExcludeReplicas` contains a list of `ReplicaSelection`s that should
 * be excluded from serving requests.
 */
class ExcludeReplicas {
 public:
  ExcludeReplicas(std::initializer_list<ReplicaSelection> replica_selections)
      : replica_selections_(replica_selections.begin(),
                            replica_selections.end()) {}

  std::vector<ReplicaSelection> const& replica_selections() const {
    return replica_selections_;
  }

 private:
  std::vector<ReplicaSelection> replica_selections_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DIRECTED_READ_REPLICAS_H
