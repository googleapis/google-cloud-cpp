// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/osconfig/agent_endpoint_connection.h"

namespace google {
namespace cloud {
namespace osconfig_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void AgentEndpointServiceReceiveTaskNotificationStreamingUpdater(
    google::cloud::osconfig::agentendpoint::v1::
        ReceiveTaskNotificationResponse const&,
    google::cloud::osconfig::agentendpoint::v1::
        ReceiveTaskNotificationRequest&) {
  // For this service, updating the streaming RPC is a no-op.  The response has
  // no information (other than its presence) and resuming just requires sending
  // the same request.
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace osconfig_internal
}  // namespace cloud
}  // namespace google
