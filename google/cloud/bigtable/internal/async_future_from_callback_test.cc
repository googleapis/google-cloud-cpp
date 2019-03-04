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

#include "google/cloud/bigtable/internal/async_future_from_callback.h"
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(AsyncFutureFromCallbackGeneric, Simple) {
  CompletionQueue cq;
  promise<StatusOr<int>> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  int value = 42;
  grpc::Status status = grpc::Status::OK;
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  StatusOr<int> res = fut.get();
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(42, *res);
}

TEST(AsyncFutureFromCallbackGeneric, Failure) {
  CompletionQueue cq;
  promise<StatusOr<int>> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  int value = 42;
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "try again");
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  StatusOr<int> res = fut.get();
  ASSERT_FALSE(res);
  ASSERT_EQ(StatusCode::kUnavailable, res.status().code());
  ASSERT_EQ("TestBody: try again", res.status().message());
}

TEST(AsyncFutureFromCallbackVoid, Simple) {
  CompletionQueue cq;
  promise<Status> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  google::protobuf::Empty value;
  grpc::Status status = grpc::Status::OK;
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  ASSERT_STATUS_OK(fut.get());
}

TEST(AsyncFutureFromCallbackVoid, Failure) {
  CompletionQueue cq;
  promise<Status> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  google::protobuf::Empty value;
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "try again");
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  Status res_status = fut.get();
  ASSERT_FALSE(res_status.ok());
  ASSERT_EQ(StatusCode::kUnavailable, res_status.code());
  ASSERT_EQ("TestBody: try again", res_status.message());
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
