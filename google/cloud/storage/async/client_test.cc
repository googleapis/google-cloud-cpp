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
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
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
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::ResultOf;
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

TEST(AsyncClient, InsertObject) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, InsertObject)
      .WillOnce([](AsyncConnection::InsertObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::IfGenerationMatch>().value_or(0),
                  42);
        return make_ready_future(make_status_or(TestObject()));
      });

  auto client = AsyncClient(mock);
  auto response = client
                      .InsertObject("test-bucket", "test-object", "Contents",
                                    storage::IfGenerationMatch(42),
                                    Options{}
                                        .set<TestOption<1>>("O1-function")
                                        .set<TestOption<2>>("O2-function"))
                      .get();
  EXPECT_THAT(response, IsOkAndHolds(TestObject()));
}

TEST(AsyncClient, ReadObject) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([](AsyncConnection::ReadObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::Generation>().value_or(0), 42);
        auto reader = std::make_unique<MockAsyncReaderConnection>();
        EXPECT_CALL(*reader, Read).WillOnce([] {
          return make_ready_future(
              AsyncReaderConnection::ReadResponse{Status{}});
        });
        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncReaderConnection>(std::move(reader))));
      });

  auto client = AsyncClient(mock);
  auto rt =
      client
          .ReadObject("test-bucket", "test-object", storage::Generation(42),
                      Options{}
                          .set<TestOption<1>>("O1-function")
                          .set<TestOption<2>>("O2-function"))
          .get();
  ASSERT_STATUS_OK(rt);
  AsyncReader r;
  AsyncToken t;
  std::tie(r, t) = *std::move(rt);
  EXPECT_TRUE(t.valid());

  auto pt = r.Read(std::move(t)).get();
  AsyncReaderConnection::ReadResponse p;
  AsyncToken t2;
  ASSERT_STATUS_OK(pt);
  std::tie(p, t2) = *std::move(pt);
  EXPECT_FALSE(t2.valid());
  EXPECT_THAT(
      p, VariantWith<ReadPayload>(ResultOf(
             "empty response", [](auto const& p) { return p.size(); }, 0)));
}

TEST(AsyncClient, ReadObjectRange) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, ReadObjectRange)
      .WillOnce([](AsyncConnection::ReadObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::Generation>().value_or(0), 42);
        auto const range = p.request.GetOption<storage::ReadRange>().value_or(
            storage::ReadRangeData{0, 0});
        EXPECT_EQ(range.begin, 100);
        EXPECT_EQ(range.end, 142);
        return make_ready_future(
            make_status_or(ReadPayload{}.set_metadata(TestObject())));
      });

  auto client = AsyncClient(mock);
  auto payload = client
                     .ReadObjectRange("test-bucket", "test-object", 100, 42,
                                      storage::Generation(42),
                                      Options{}
                                          .set<TestOption<1>>("O1-function")
                                          .set<TestOption<2>>("O2-function"))
                     .get();
  ASSERT_STATUS_OK(payload);
  EXPECT_THAT(payload->metadata(), Optional(Eq(TestObject())));
}

TEST(AsyncClient, StartUnbufferedUpload) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartUnbufferedUpload)
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
                .StartUnbufferedUpload("test-bucket", "test-object",
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

TEST(AsyncClient, StartUnbufferedUploadResumeFinalized) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(Options{}));
  EXPECT_CALL(*mock, StartUnbufferedUpload).WillOnce([] {
    auto writer = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*writer, PersistedState).WillRepeatedly(Return(TestObject()));

    return make_ready_future(make_status_or(
        std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
  });

  auto client = AsyncClient(mock);
  auto wt = client.StartUnbufferedUpload("test-bucket", "test-object").get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
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

TEST(AsyncClient, ComposeObject) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](AsyncConnection::ComposeObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        auto source_is = [](std::string name) {
          return ResultOf(
              "source object name", [](auto const& s) { return s.object_name; },
              std::move(name));
        };
        EXPECT_THAT(p.request.source_objects(),
                    ElementsAre(source_is("source0"), source_is("source1")));
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::IfGenerationMatch>().value_or(0),
                  42);
        return make_ready_future(make_status_or(TestObject()));
      });

  auto client = AsyncClient(mock);
  auto response =
      client
          .ComposeObject("test-bucket",
                         {storage::ComposeSourceObject{"source0", absl::nullopt,
                                                       absl::nullopt},
                          storage::ComposeSourceObject{"source1", absl::nullopt,
                                                       absl::nullopt}},
                         "test-object", storage::IfGenerationMatch(42),
                         Options{}
                             .set<TestOption<1>>("O1-function")
                             .set<TestOption<2>>("O2-function"))
          .get();
  EXPECT_THAT(response, IsOkAndHolds(TestObject()));
}

TEST(AsyncClient, DeleteObject) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](AsyncConnection::DeleteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        EXPECT_EQ(p.request.bucket_name(), "test-bucket");
        EXPECT_EQ(p.request.object_name(), "test-object");
        EXPECT_EQ(p.request.GetOption<storage::IfGenerationMatch>().value_or(0),
                  42);
        return make_ready_future(Status{});
      });

  auto client = AsyncClient(mock);
  auto response = client
                      .DeleteObject("test-bucket", "test-object",
                                    storage::IfGenerationMatch(42),
                                    Options{}
                                        .set<TestOption<1>>("O1-function")
                                        .set<TestOption<2>>("O2-function"))
                      .get();
  EXPECT_THAT(response, IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
