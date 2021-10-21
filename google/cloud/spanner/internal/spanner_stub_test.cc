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

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::HasSubstr;

TEST(SpannerStub, CreateDefaultStub) {
  auto stub = CreateDefaultSpannerStub(spanner::Database("foo", "bar", "baz"),
                                       spanner_internal::DefaultOptions(),
                                       /*channel_id=*/0);
  EXPECT_NE(stub, nullptr);
}

TEST(SpannerStub, CreateDefaultStubWithLogging) {
  testing_util::ScopedLog log;

  auto opts = Options{}
                  .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
                  .set<EndpointOption>("localhost:1")
                  .set<TracingComponentsOption>({"rpc"});
  auto stub =
      CreateDefaultSpannerStub(spanner::Database("foo", "bar", "baz"), opts,
                               /*channel_id=*/0);
  EXPECT_NE(stub, nullptr);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(5));
  auto session =
      stub->CreateSession(context, google::spanner::v1::CreateSessionRequest());
  EXPECT_THAT(session, StatusIs(AnyOf(StatusCode::kUnavailable,
                                      StatusCode::kInvalidArgument,
                                      StatusCode::kDeadlineExceeded)));

  EXPECT_THAT(log.ExtractLines(),
              Contains(HasSubstr(session.status().message())));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
