// Copyright 2018 Google LLC.
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

#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace {

namespace btadmin = ::google::bigtable::admin::v2;
using ::testing::_;
using ::testing::Invoke;

class MockClient {
 public:
  MOCK_METHOD3(
      AsyncGetTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(
          grpc::ClientContext*, btadmin::GetTableRequest const&,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(AsyncDeleteTable,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::protobuf::Empty>>(grpc::ClientContext*,
                                             btadmin::DeleteTableRequest const&,
                                             grpc::CompletionQueue* cq));
};

TEST(AsyncRetryUnaryRpcTest, ImmediatelySucceeds) {
  using namespace google::cloud::testing_util::chrono_literals;

  MockClient client;

  using ReaderType =
      ::google::cloud::bigtable::testing::MockAsyncResponseReader<
          btadmin::Table>;
  auto reader = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::Table* table, grpc::Status* status, void*) {
        // Initialize a value to make sure it is carried all the way back to
        // the caller.
        table->set_name("fake/table/name/response");
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(client, AsyncGetTable(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btadmin::GetTableRequest const& request,
                                 grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btadmin::Table>>(reader.get());
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::GetTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      __func__, LimitedErrorCountRetryPolicy(3).clone(),
      ExponentialBackoffPolicy(10_us, 40_us).clone(),
      ConstantIdempotencyPolicy(true),
      MetadataUpdatePolicy("resource", MetadataParamTypes::RESOURCE),
      [&client](grpc::ClientContext* context,
                btadmin::GetTableRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetTable(context, request, cq);
      },
      request, cq);

  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(impl->empty());
  EXPECT_EQ(std::future_status::ready, fut.wait_for(0_us));
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
