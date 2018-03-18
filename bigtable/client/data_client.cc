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
#include "bigtable/client/internal/common_client.h"

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implement a simple DataClient.
 *
 * This implementation does not support multiple threads, or refresh
 * authorization tokens.  In other words, it is extremely bare bones.
 */
class DefaultDataClient : public DataClient {
 private:
  // Introduce an early `private:` section because this type is used to define
  // the public interface, it should not be part of the public interface.
  struct DataTraits {
    static std::string const& Endpoint(bigtable::ClientOptions& options) {
      return options.data_endpoint();
    }
  };

  using Impl =
      bigtable::internal::CommonClient<DataTraits,
                                       ::google::bigtable::v2::Bigtable>;

 public:
  DefaultDataClient(std::string project, std::string instance,
                    ClientOptions options)
      : project_(std::move(project)),
        instance_(std::move(instance)),
        impl_(std::move(options)) {}

  DefaultDataClient(std::string project, std::string instance)
      : DefaultDataClient(std::move(project), std::move(instance),
                          ClientOptions()) {}

  std::string const& project_id() const override;
  std::string const& instance_id() const override;

  using BigtableStubPtr =
      std::shared_ptr<google::bigtable::v2::Bigtable::StubInterface>;

  BigtableStubPtr Stub() override { return impl_.Stub(); }
  void reset() override { impl_.reset(); }
  void on_completion(grpc::Status const& status) override {}

 private:
  std::string project_;
  std::string instance_;
  Impl impl_;
};

std::string const& DefaultDataClient::project_id() const { return project_; }

std::string const& DefaultDataClient::instance_id() const { return instance_; }

std::shared_ptr<DataClient> CreateDefaultDataClient(std::string project_id,
                                                    std::string instance_id,
                                                    ClientOptions options) {
  return std::make_shared<DefaultDataClient>(
      std::move(project_id), std::move(instance_id), std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
