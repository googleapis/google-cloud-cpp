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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_INSTANCES_DATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_INSTANCES_DATA_H_

#include "google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h"
//#include "google/cloud/bigtable/completion_queue.h"
//#include "google/cloud/bigtable/instance_admin_client.h"
//#include "google/cloud/bigtable/internal/async_retry_multi_page.h"
//#include "google/cloud/bigtable/metadata_update_policy.h"
//#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The result of an async list instances.
 *
 * Callbacks async list instances will use for response.
 */
struct InstanceList {
  std::vector<google::bigtable::admin::v2::Instance> instances;
  std::vector<std::string> failed_locations;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_INSTANCES_DATA_H_
