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

#include "google/cloud/bigquery/connection_options.h"
#include "google/cloud/bigquery/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {
std::string BaseUserAgentPrefix() {
  // TODO(aryann): Add more info here.
  return "gcloud-cpp/" + VersionString();
}
}  // namespace internal

ConnectionOptions::ConnectionOptions(
    std::shared_ptr<grpc::ChannelCredentials> credentials)
    : credentials_(std::move(credentials)),
      bigquerystorage_endpoint_("bigquerystorage.googleapis.com"),
      user_agent_prefix_(internal::BaseUserAgentPrefix()) {}

ConnectionOptions::ConnectionOptions()
    : ConnectionOptions(grpc::GoogleDefaultCredentials()) {}

grpc::ChannelArguments ConnectionOptions::CreateChannelArguments() const {
  grpc::ChannelArguments channel_arguments;

  channel_arguments.SetUserAgentPrefix(user_agent_prefix());
  return channel_arguments;
}

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
