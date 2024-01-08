// Copyright 2023 Google LLC
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

#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::testing::Return;
using ::testing::VariantWith;

template <int N>
struct TestOption {
  using Type = std::string;
};

storage::ObjectMetadata TestObject() {
  return storage::ObjectMetadata{}
      .set_bucket("test-bucket")
      .set_name("test-object")
      .set_size(0);
}

TEST(AsyncClient, StartBufferedUpload) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartBufferedUpload)
      .WillOnce([](AsyncConnection::UploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::IfGenerationMatch>().value_or(0),
                  42);
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState).WillOnce(Return(0));
        EXPECT_CALL(*writer, Finalize).WillRepeatedly([] {
          return make_ready_future(make_status_or(TestObject()));
        });
        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto wt = client
                .StartBufferedUpload("test-bucket", "test-object",
                                     storage::IfGenerationMatch(42),
                                     Options{}
                                         .set<TestOption<1>>("O1-function")
                                         .set<TestOption<2>>("O2-function"))
                .get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_TRUE(t.valid());
  auto object = w.Finalize(std::move(t)).get();
  EXPECT_THAT(object, IsOkAndHolds(TestObject()));
}

TEST(AsyncClient, StartBufferedUploadResumeFinalized) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(Options{}));
  EXPECT_CALL(*mock, StartBufferedUpload).WillOnce([] {
    auto writer = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*writer, PersistedState).WillRepeatedly(Return(TestObject()));

    return make_ready_future(make_status_or(
        std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
  });

  auto client = AsyncClient(mock);
  auto wt = client.StartBufferedUpload("test-bucket", "test-object").get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
