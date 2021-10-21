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

#include "google/cloud/bigtable/testing/embedded_server_test_fixture.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/build_info.h"
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
  builder_.RegisterService(&admin_service_);
  server_ = builder_.BuildAndStart();
  wait_thread_ = std::thread([this]() { server_->Wait(); });
}

void EmbeddedServerTestFixture::SetUp() {
  StartServer();

  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetUserAgentPrefix(ClientOptions::UserAgentPrefix());
  std::string project_id = kProjectId;
  std::string instance_id = kInstanceId;
  std::string table_id = kTableId;

  std::shared_ptr<grpc::Channel> data_channel =
      server_->InProcessChannel(channel_arguments);
  data_client_ = std::make_shared<InProcessDataClient>(project_id, instance_id,
                                                       std::move(data_channel));
  table_ = std::make_shared<bigtable::Table>(data_client_, std::move(table_id));

  std::shared_ptr<grpc::Channel> admin_channel =
      server_->InProcessChannel(channel_arguments);
  admin_client_ = std::make_shared<InProcessAdminClient>(
      std::move(project_id), std::move(admin_channel));
  admin_ = std::make_shared<bigtable::TableAdmin>(admin_client_,
                                                  std::move(instance_id));
}

void EmbeddedServerTestFixture::TearDown() {
  server_->Shutdown();
  wait_thread_.join();
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
