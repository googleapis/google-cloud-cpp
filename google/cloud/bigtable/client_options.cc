// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "absl/strings/str_split.h"
#include <limits>
#include <thread>

namespace {
// For background information on gRPC keepalive pings, see
//     https://github.com/grpc/grpc/blob/master/doc/keepalive.md

// The default value for GRPC_KEEPALIVE_TIME_MS, how long before a keepalive
// ping is sent. A better name may have been "period", but consistency with the
// gRPC naming seems valuable.
auto constexpr kDefaultKeepaliveTime =
    std::chrono::milliseconds(std::chrono::seconds(30));

// The default value for GRPC_KEEPALIVE_TIME_MS, how long the sender (in this
// case the Cloud Bigtable C++ client library) waits for an acknowledgement for
// a keepalive ping.
auto constexpr kDefaultKeepaliveTimeout =
    std::chrono::milliseconds(std::chrono::seconds(10));

}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

ClientOptions::ClientOptions() : ClientOptions(Options{}) {}

ClientOptions::ClientOptions(Options opts) {
  ::google::cloud::internal::CheckExpectedOptions<
      ClientOptionList, CommonOptionList, GrpcOptionList>(opts, __func__);
  opts_ = internal::DefaultOptions(std::move(opts));

  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  channel_arguments_.SetUserAgentPrefix(UserAgentPrefix());
  channel_arguments_.SetMaxSendMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  channel_arguments_.SetMaxReceiveMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  auto to_arg = [](std::chrono::milliseconds ms) {
    auto const count = ms.count();
    if (count >= (std::numeric_limits<int>::max)()) {
      return std::numeric_limits<int>::max();
    }
    if (count <= (std::numeric_limits<int>::min)()) {
      return std::numeric_limits<int>::min();
    }
    return static_cast<int>(ms.count());
  };
  channel_arguments_.SetInt(
      GRPC_ARG_KEEPALIVE_TIME_MS,
      to_arg(duration_cast<milliseconds>(kDefaultKeepaliveTime)));
  channel_arguments_.SetInt(
      GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
      to_arg(duration_cast<milliseconds>(kDefaultKeepaliveTimeout)));
}

ClientOptions::ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds)
    : ClientOptions(Options{}.set<GrpcCredentialOption>(std::move(creds))) {
  set_data_endpoint("bigtable.googleapis.com");
  set_admin_endpoint("bigtableadmin.googleapis.com");
}

// NOLINTNEXTLINE(readability-identifier-naming)
ClientOptions& ClientOptions::set_connection_pool_size(std::size_t size) {
  opts_.set<GrpcNumChannelsOption>(
      size == 0 ? internal::DefaultConnectionPoolSize() : int(size));
  return *this;
}

std::string ClientOptions::UserAgentPrefix() {
  return google::cloud::internal::UserAgentPrefix();
}

ClientOptions& ClientOptions::DisableBackgroundThreads(
    google::cloud::CompletionQueue const& cq) {
  opts_.set<GrpcBackgroundThreadsFactoryOption>([cq] {
    return absl::make_unique<
        google::cloud::internal::CustomerSuppliedBackgroundThreads>(cq);
  });
  return *this;
}

BackgroundThreadsFactory ClientOptions::background_threads_factory() const {
  return google::cloud::internal::MakeBackgroundThreadsFactory(opts_);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
