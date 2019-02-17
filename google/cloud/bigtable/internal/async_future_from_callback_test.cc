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
  promise<int> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  int value = 42;
  grpc::Status status = grpc::Status::OK;
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  EXPECT_EQ(42, fut.get());
}

TEST(AsyncFutureFromCallbackGeneric, Failure) {
  CompletionQueue cq;
  promise<int> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  int value = 42;
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "try again");
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { fut.get(); } catch (GRpcError const& ex) {
    EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, ex.error_code());
    EXPECT_THAT(ex.what(), HasSubstr("try again"));
    throw;
  },
               GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(fut.get(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(AsyncFutureFromCallbackVoid, Simple) {
  CompletionQueue cq;
  promise<void> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  google::protobuf::Empty value;
  grpc::Status status = grpc::Status::OK;
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
  fut.get();
  SUCCEED();
}

TEST(AsyncFutureFromCallbackVoid, Failure) {
  CompletionQueue cq;
  promise<void> p;
  auto fut = p.get_future();
  auto callback = MakeAsyncFutureFromCallback(std::move(p), __func__);
  EXPECT_FALSE(fut.is_ready());
  google::protobuf::Empty value;
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "try again");
  callback(cq, value, status);

  ASSERT_TRUE(fut.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { fut.get(); } catch (GRpcError const& ex) {
    EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, ex.error_code());
    EXPECT_THAT(ex.what(), HasSubstr("try again"));
    throw;
  },
               GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(fut.get(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
