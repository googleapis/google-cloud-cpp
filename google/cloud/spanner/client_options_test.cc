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

#include "google/cloud/spanner/client_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {
TEST(ClientOptionsTest, Credentials) {
  // In the CI environment grpc::GoogleDefaultCredentials() may assert. Use the
  // insecure credentials to initialize the options in any unit test.
  auto expected = grpc::InsecureChannelCredentials();
  ClientOptions options(expected);
  EXPECT_EQ(expected.get(), options.credentials().get());
}

TEST(ClientOptionsTest, AdminEndpoint) {
  ClientOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ("spanner.googleapis.com", options.admin_endpoint());
  options.set_admin_endpoint("invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", options.admin_endpoint());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
