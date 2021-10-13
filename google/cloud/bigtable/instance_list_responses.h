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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_LIST_RESPONSES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_LIST_RESPONSES_H

#include "google/cloud/bigtable/version.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * The response for an asynchronous request listing all the instances.
 */
struct InstanceList {
  /// The list of instances received from Cloud Bigtable.
  std::vector<google::bigtable::admin::v2::Instance> instances;

  /**
   * The list of Google Cloud Platform locations where the request could not
   * get a response from.
   *
   * During an outage Cloud Bigtable may be unable to access specific zones. In
   * that case the service will return those locations for which no information
   * could be retrieved in this parameter.
   */
  std::vector<std::string> failed_locations;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_LIST_RESPONSES_H
