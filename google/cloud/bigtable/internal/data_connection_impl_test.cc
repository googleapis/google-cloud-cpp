// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;

class DataConnectionTest : public ::testing::Test {
 protected:
  std::shared_ptr<DataConnectionImpl> TestConnection() {
    auto options =
        Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
    auto background = internal::MakeBackgroundThreadsFactory(options)();
    return std::make_shared<DataConnectionImpl>(std::move(background),
                                                mock_stub_, std::move(options));
  }

  std::shared_ptr<MockBigtableStub> mock_stub_ =
      std::make_shared<MockBigtableStub>();
};

TEST_F(DataConnectionTest, Options) {
  auto connection = TestConnection();
  auto options = connection->options();
  EXPECT_TRUE(options.has<GrpcCredentialOption>());
  ASSERT_TRUE(options.has<EndpointOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
