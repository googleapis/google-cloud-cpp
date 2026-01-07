// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/testing/embedded_server_test_fixture.h"
#include "google/cloud/bigtable/internal/bigtable_metadata_decorator.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/retry_policy.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

char const EmbeddedServerTestFixture::kProjectId[] = "foo-project";
char const EmbeddedServerTestFixture::kInstanceId[] = "bar-instance";
char const EmbeddedServerTestFixture::kTableId[] = "baz-table";

// These are hardcoded, and not computed, because we want to test the
// computation.
char const EmbeddedServerTestFixture::kInstanceName[] =
    "projects/foo-project/instances/bar-instance";
char const EmbeddedServerTestFixture::kTableName[] =
    "projects/foo-project/instances/bar-instance/tables/baz-table";

void EmbeddedServerTestFixture::StartServer() {
  int port;
  std::string server_address("[::]:0");
  builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                            &port);
  builder_.RegisterService(&bigtable_service_);
  server_ = builder_.BuildAndStart();
  wait_thread_ = std::thread([this]() { server_->Wait(); });
}

void EmbeddedServerTestFixture::SetUp() {
  StartServer();

  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetUserAgentPrefix(
      google::cloud::internal::UserAgentPrefix());

  std::shared_ptr<grpc::Channel> data_channel =
      server_->InProcessChannel(channel_arguments);
  std::unique_ptr<google::bigtable::v2::Bigtable::StubInterface> grpc_stub =
      google::bigtable::v2::Bigtable::NewStub(data_channel);
  std::shared_ptr<bigtable_internal::BigtableStub> stub =
      std::make_shared<bigtable_internal::BigtableMetadata>(
          std::make_shared<bigtable_internal::DefaultBigtableStub>(
              std::move(grpc_stub)),
          std::multimap<std::string, std::string>{},
          google::cloud::internal::UserAgentPrefix());
  auto opts =
      Options{}
          .set<google::cloud::bigtable::DataRetryPolicyOption>(
              google::cloud::bigtable::DataLimitedErrorCountRetryPolicy(7)
                  .clone())
          .set<google::cloud::bigtable::DataBackoffPolicyOption>(
              google::cloud::ExponentialBackoffPolicy(
                  /*initial_delay=*/std::chrono::milliseconds(200),
                  /*maximum_delay=*/std::chrono::seconds(45),
                  /*scaling=*/2.0)
                  .clone());
  data_connection_ = std::make_shared<bigtable_internal::DataConnectionImpl>(
      std::make_unique<
          google::cloud::internal::AutomaticallyCreatedBackgroundThreads>(),
      stub, std::make_shared<bigtable_internal::NoopMutateRowsLimiter>(),
      std::move(opts));

  table_ = std::make_shared<bigtable::Table>(
      data_connection_,
      bigtable::TableResource(kProjectId, kInstanceId, kTableId));
}

void EmbeddedServerTestFixture::TearDown() {
  server_->Shutdown();
  wait_thread_.join();
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
