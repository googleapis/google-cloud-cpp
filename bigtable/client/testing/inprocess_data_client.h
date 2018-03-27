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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_DATA_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_DATA_CLIENT_H_

#include "bigtable/client/client_options.h"
#include "bigtable/client/data_client.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
namespace testing {

/**
 * Connect to an embedded Cloud Bigtable server implementing the data
 * manipulation APIs.
 *
 * This class is mainly for testing purpose, it enable use of a single embedded
 * server
 * for multiple test cases. This dataclient uses a pre-defined channel.
 */
class InProcessDataClient : public bigtable::DataClient {
 public:
  InProcessDataClient(std::string project, std::string instance,
                      std::shared_ptr<grpc::Channel> channel)
      : project_(std::move(project)),
        instance_(std::move(instance)),
        channel_(std::move(channel)) {}

  using BigtableStubPtr =
      std::shared_ptr<google::bigtable::v2::Bigtable::StubInterface>;

  std::string const& project_id() const override { return project_; }
  std::string const& instance_id() const override { return instance_; }
  BigtableStubPtr Stub() override {
    return btproto::Bigtable::NewStub(channel_);
  }
  void reset() override {}
  void on_completion(grpc::Status const& status) override {}

 private:
  std::string project_;
  std::string instance_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_INPROCESS_DATA_CLIENT_H_
