// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "absl/strings/str_split.h"
#include <limits>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
Options&& MakeOptions(ClientOptions&& o) {
  if (!o.connection_pool_name_.empty()) {
    o.opts_
        .lookup<GrpcChannelArgumentsOption>()["cbt-c++/connection-pool-name"] =
        std::move(o.connection_pool_name_);
  }
  return std::move(o.opts_);
}
}  // namespace internal

ClientOptions::ClientOptions() : ClientOptions(Options{}) {}

ClientOptions::ClientOptions(Options opts) {
  ::google::cloud::internal::CheckExpectedOptions<
      ClientOptionList, CommonOptionList, GrpcOptionList>(opts, __func__);
  opts_ = internal::DefaultOptions(std::move(opts));
}

ClientOptions::ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds)
    : ClientOptions(Options{}.set<GrpcCredentialOption>(std::move(creds))) {
  set_data_endpoint("bigtable.googleapis.com");
  set_admin_endpoint("bigtableadmin.googleapis.com");
}

// NOLINTNEXTLINE(readability-identifier-naming)
ClientOptions& ClientOptions::set_connection_pool_size(std::size_t size) {
  opts_.set<GrpcNumChannelsOption>(size == 0
                                       ? internal::DefaultConnectionPoolSize()
                                       : static_cast<int>(size));
  return *this;
}

std::string ClientOptions::UserAgentPrefix() {
  return google::cloud::internal::UserAgentPrefix();
}

ClientOptions& ClientOptions::DisableBackgroundThreads(
    google::cloud::CompletionQueue const& cq) {
  opts_.set<GrpcCompletionQueueOption>(cq);
  return *this;
}

BackgroundThreadsFactory ClientOptions::background_threads_factory() const {
  return google::cloud::internal::MakeBackgroundThreadsFactory(opts_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
