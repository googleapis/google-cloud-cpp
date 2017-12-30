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
// limitations under the License.

#include "bigtable/client/data_client.h"

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implement a simple DataClient.
 *
 * This implementation does not support multiple threads, handle GOAWAY
 * responses, nor does it refresh authorization tokens.  In other words, it is
 * extremely bare bones.
 */
class DefaultDataClient : public DataClient {
 public:
  DefaultDataClient(std::string project, std::string instance,
                    ClientOptions options)
      : project_(std::move(project)),
        instance_(std::move(instance)),
        credentials_(options.credentials()),
        channel_(grpc::CreateChannel(options.data_endpoint(),
                                     options.credentials())),
        bt_stub_(google::bigtable::v2::Bigtable::NewStub(channel_)) {}

  DefaultDataClient(std::string project, std::string instance)
      : DefaultDataClient(std::move(project), std::move(instance),
                          ClientOptions()) {}

  std::string const& project_id() const override;
  std::string const& instance_id() const override;

  google::bigtable::v2::Bigtable::StubInterface& Stub() const override {
    return *bt_stub_;
  }

 private:
  std::string project_;
  std::string instance_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<google::bigtable::v2::Bigtable::StubInterface> bt_stub_;
};

std::string const& DefaultDataClient::project_id() const { return project_; }

std::string const& DefaultDataClient::instance_id() const { return instance_; }

std::shared_ptr<DataClient> CreateDefaultClient(
    std::string project_id, std::string instance_id,
    bigtable::ClientOptions options) {
  return std::make_shared<DefaultDataClient>(
      std::move(project_id), std::move(instance_id), std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
