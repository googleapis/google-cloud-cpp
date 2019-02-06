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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_LIST_RESPONSES_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_LIST_RESPONSES_H_

#include "google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h"
#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The response for an asynchronous request listing all the clusters.
 */
struct ClusterList {
  /// The list of clusters received from Cloud Bigtable.
  std::vector<google::bigtable::admin::v2::Cluster> clusters;

  /// The list of Google Cloud Platform locations where the request could not
  /// get a response from.
  std::vector<std::string> failed_locations;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLUSTER_LIST_RESPONSES_H_
