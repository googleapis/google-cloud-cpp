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

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/internal/background_threads_impl.h"
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
std::string BaseUserAgentPrefix() {
  return "gcloud-cpp/" + google::cloud::spanner::VersionString() + " (" +
         CompilerId() + "-" + CompilerVersion() + "; " + CompilerFeatures() +
         ")";
}
}  // namespace internal

ConnectionOptions::ConnectionOptions(
    std::shared_ptr<grpc::ChannelCredentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("spanner.googleapis.com"),
      num_channels_(4),
      user_agent_prefix_(internal::BaseUserAgentPrefix()),
      background_threads_factory_([] {
        return google::cloud::internal::make_unique<
            internal::AutomaticallyCreatedBackgroundThreads>();
      }) {
  auto tracing =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  if (tracing.has_value()) {
    std::istringstream is{*tracing};
    std::string token;
    while (std::getline(is, token, ',')) {
      tracing_components_.insert(token);
    }
  }

  if (google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_CLOG")
          .has_value()) {
    google::cloud::LogSink::EnableStdClog();
  }
}

ConnectionOptions::ConnectionOptions()
    : ConnectionOptions(grpc::GoogleDefaultCredentials()) {}

grpc::ChannelArguments ConnectionOptions::CreateChannelArguments() const {
  grpc::ChannelArguments channel_arguments;

  if (!channel_pool_domain().empty()) {
    // To get a different channel pool one just needs to set any channel
    // parameter to a different value. Newer versions of gRPC include a macro
    // for this purpose (GRPC_ARG_CHANNEL_POOL_DOMAIN). As we are compiling
    // against older versions in some cases, we use the actual value.
    channel_arguments.SetString("grpc.channel_pooling_domain",
                                channel_pool_domain());
  }
  channel_arguments.SetUserAgentPrefix(user_agent_prefix());
  return channel_arguments;
}

ConnectionOptions& ConnectionOptions::DisableBackgroundThreads(
    google::cloud::grpc_utils::CompletionQueue const& cq) {
  background_threads_factory_ = [cq] {
    return google::cloud::internal::make_unique<
        internal::CustomerSuppliedBackgroundThreads>(cq);
  };
  return *this;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
