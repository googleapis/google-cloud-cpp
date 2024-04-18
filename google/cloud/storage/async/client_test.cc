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
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
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

auto TestProtoObject() {
  google::storage::v2::Object result;
  result.set_bucket("projects/_/buckets/test-bucket");
  result.set_name("test-object");
  result.set_size(0);
  return result;
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

TEST(AsyncClient, StartBufferedUpload1) {
  auto constexpr kExpectedRequest = R"pb(
    write_object_spec {
      resource { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartBufferedUpload)
      .WillOnce([](AsyncConnection::UploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::StartResumableWriteRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
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
                .StartBufferedUpload(BucketName("test-bucket"), "test-object",
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

TEST(AsyncClient, StartBufferedUpload2) {
  auto constexpr kExpectedRequest = R"pb(
    write_object_spec {
      resource { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
      if_generation_match: 42
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartBufferedUpload)
      .WillOnce([](AsyncConnection::UploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::StartResumableWriteRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState).WillOnce(Return(0));
        EXPECT_CALL(*writer, Finalize).WillRepeatedly([] {
          return make_ready_future(make_status_or(TestObject()));
        });
        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto request = google::storage::v2::StartResumableWriteRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto wt = client
                .StartBufferedUpload(std::move(request),
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

TEST(AsyncClient, ResumeBufferedUpload1) {
  auto constexpr kExpectedRequest = R"pb(
    upload_id: "resume-upload-id"
  )pb";

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));
  EXPECT_CALL(*mock, ResumeBufferedUpload)
      .WillOnce([&](AsyncConnection::ResumeUploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::QueryWriteStatusRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState)
            .WillRepeatedly(Return(TestObject()));

        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto wt = client
                .ResumeBufferedUpload("resume-upload-id",
                                      Options{}
                                          .set<TestOption<1>>("O1-function")
                                          .set<TestOption<2>>("O2-function"))
                .get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

TEST(AsyncClient, ResumeBufferedUpload2) {
  auto constexpr kExpectedRequest = R"pb(
    upload_id: "resume-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-only-invalid-encryption-algorigthm"
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));
  EXPECT_CALL(*mock, ResumeBufferedUpload)
      .WillOnce([&](AsyncConnection::ResumeUploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::QueryWriteStatusRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState)
            .WillRepeatedly(Return(TestObject()));

        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto wt =
      client
          .ResumeBufferedUpload(request, Options{}
                                             .set<TestOption<1>>("O1-function")
                                             .set<TestOption<2>>("O2-function"))
          .get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

TEST(AsyncClient, ResumeBufferedUploadResumeFinalized) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(Options{}));
  EXPECT_CALL(*mock, ResumeBufferedUpload).WillOnce([] {
    auto writer = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*writer, PersistedState).WillRepeatedly(Return(TestObject()));

    return make_ready_future(make_status_or(
        std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
  });

  auto client = AsyncClient(mock);
  auto wt = client.ResumeBufferedUpload("resume-upload-id").get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
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

TEST(AsyncClient, StartUnbufferedUpload1) {
  auto constexpr kExpectedRequest = R"pb(
    write_object_spec {
      resource { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartUnbufferedUpload)
      .WillOnce([](AsyncConnection::UploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::StartResumableWriteRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState).WillOnce(Return(0));
        EXPECT_CALL(*writer, Finalize).WillRepeatedly([] {
          return make_ready_future(make_status_or(TestObject()));
        });
        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto request = google::storage::v2::StartResumableWriteRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto wt = client
                .StartUnbufferedUpload(BucketName("test-bucket"), "test-object",
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

TEST(AsyncClient, StartUnbufferedUpload2) {
  auto constexpr kExpectedRequest = R"pb(
    write_object_spec {
      resource { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
      if_generation_match: 42
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, StartUnbufferedUpload)
      .WillOnce([](AsyncConnection::UploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::StartResumableWriteRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState).WillOnce(Return(0));
        EXPECT_CALL(*writer, Finalize).WillRepeatedly([] {
          return make_ready_future(make_status_or(TestObject()));
        });
        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto request = google::storage::v2::StartResumableWriteRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto wt = client
                .StartUnbufferedUpload(std::move(request),
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

TEST(AsyncClient, ResumeUnbufferedUpload1) {
  auto constexpr kExpectedRequest = R"pb(
    upload_id: "resume-upload-id"
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload)
      .WillOnce([&](AsyncConnection::ResumeUploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::QueryWriteStatusRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState)
            .WillRepeatedly(Return(TestObject()));

        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto wt = client
                .ResumeUnbufferedUpload("resume-upload-id",
                                        Options{}
                                            .set<TestOption<1>>("O1-function")
                                            .set<TestOption<2>>("O2-function"))
                .get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

TEST(AsyncClient, ResumeUnbufferedUpload2) {
  auto constexpr kExpectedRequest = R"pb(
    upload_id: "resume-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-only-invalid-encryption-algorigthm"
    }
  )pb";
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload)
      .WillOnce([&](AsyncConnection::ResumeUploadParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto expected = google::storage::v2::QueryWriteStatusRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        auto writer = std::make_unique<MockAsyncWriterConnection>();
        EXPECT_CALL(*writer, PersistedState)
            .WillRepeatedly(Return(TestObject()));

        return make_ready_future(make_status_or(
            std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
      });

  auto client = AsyncClient(mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto wt = client
                .ResumeUnbufferedUpload(request,
                                        Options{}
                                            .set<TestOption<1>>("O1-function")
                                            .set<TestOption<2>>("O2-function"))
                .get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

TEST(AsyncClient, ResumeUnbufferedUploadResumeFinalized) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(Options{}));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload).WillOnce([] {
    auto writer = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*writer, PersistedState).WillRepeatedly(Return(TestObject()));

    return make_ready_future(make_status_or(
        std::unique_ptr<AsyncWriterConnection>(std::move(writer))));
  });

  auto client = AsyncClient(mock);
  auto wt = client.ResumeUnbufferedUpload("resume-upload-id").get();
  ASSERT_STATUS_OK(wt);
  AsyncWriter w;
  AsyncToken t;
  std::tie(w, t) = *std::move(wt);
  EXPECT_FALSE(t.valid());
  EXPECT_THAT(w.PersistedState(),
              VariantWith<storage::ObjectMetadata>(TestObject()));
}

TEST(AsyncClient, ComposeObject1) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](AsyncConnection::ComposeObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          destination {
            bucket: "projects/_/buckets/test-bucket"
            name: "test-object"
          }
          source_objects { name: "source0" }
          source_objects { name: "source1" }
        )pb";
        google::storage::v2::ComposeObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        return make_ready_future(make_status_or(TestProtoObject()));
      });

  auto client = AsyncClient(mock);
  auto make_source = [](std::string n) {
    google::storage::v2::ComposeObjectRequest::SourceObject source;
    source.set_name(std::move(n));
    return source;
  };
  auto response =
      client
          .ComposeObject(BucketName("test-bucket"), "test-object",
                         {make_source("source0"), make_source("source1")},
                         Options{}
                             .set<TestOption<1>>("O1-function")
                             .set<TestOption<2>>("O2-function"))
          .get();
  EXPECT_THAT(response, IsOkAndHolds(IsProtoEqual(TestProtoObject())));
}

TEST(AsyncClient, ComposeObject2) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](AsyncConnection::ComposeObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          destination {
            bucket: "test-only-invalid"
            name: "test-object"
            content_type: "text/plain"
          }
          if_generation_match: 12345
          source_objects { name: "source0" generation: 2345 }
          source_objects { name: "source1" generation: 2345 }
        )pb";
        google::storage::v2::ComposeObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        return make_ready_future(make_status_or(TestProtoObject()));
      });

  auto client = AsyncClient(mock);
  auto make_source = [](std::string n) {
    google::storage::v2::ComposeObjectRequest::SourceObject source;
    source.set_name(std::move(n));
    source.set_generation(2345);
    return source;
  };
  google::storage::v2::ComposeObjectRequest request;
  request.mutable_destination()->set_bucket("test-only-invalid");
  request.mutable_destination()->set_name("test-object");
  request.mutable_destination()->set_content_type("text/plain");
  request.set_if_generation_match(12345);
  request.mutable_source_objects()->Add(make_source("source0"));
  request.mutable_source_objects()->Add(make_source("source1"));
  auto response = client
                      .ComposeObject(std::move(request),
                                     Options{}
                                         .set<TestOption<1>>("O1-function")
                                         .set<TestOption<2>>("O2-function"))
                      .get();
  EXPECT_THAT(response, IsOkAndHolds(IsProtoEqual(TestProtoObject())));
}

TEST(AsyncClient, DeleteObject1) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](AsyncConnection::DeleteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          bucket: "projects/_/buckets/test-bucket"
          object: "test-object"
        )pb";
        google::storage::v2::DeleteObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        return make_ready_future(Status{});
      });

  auto client = AsyncClient(mock);
  auto response = client
                      .DeleteObject(BucketName("test-bucket"), "test-object",
                                    Options{}
                                        .set<TestOption<1>>("O1-function")
                                        .set<TestOption<2>>("O2-function"))
                      .get();
  EXPECT_THAT(response, IsOk());
}

TEST(AsyncClient, DeleteObject2) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](AsyncConnection::DeleteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          bucket: "projects/_/buckets/test-bucket"
          object: "test-object"
          generation: 12345
        )pb";
        google::storage::v2::DeleteObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        return make_ready_future(Status{});
      });

  auto client = AsyncClient(mock);
  auto response =
      client
          .DeleteObject(BucketName("test-bucket"), "test-object", 12345,
                        Options{}
                            .set<TestOption<1>>("O1-function")
                            .set<TestOption<2>>("O2-function"))
          .get();
  EXPECT_THAT(response, IsOk());
}

TEST(AsyncClient, DeleteObject3) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](AsyncConnection::DeleteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          bucket: "test-only-invalid"
          object: "test-object"
          if_generation_match: 42
        )pb";
        google::storage::v2::DeleteObjectRequest expected;
        EXPECT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
        EXPECT_THAT(p.request, IsProtoEqual(expected));
        return make_ready_future(Status{});
      });

  auto client = AsyncClient(mock);
  google::storage::v2::DeleteObjectRequest request;
  request.set_bucket("test-only-invalid");
  request.set_object("test-object");
  request.set_if_generation_match(42);
  auto response = client
                      .DeleteObject(std::move(request),
                                    Options{}
                                        .set<TestOption<1>>("O1-function")
                                        .set<TestOption<2>>("O2-function"))
                      .get();
  EXPECT_THAT(response, IsOk());
}

using ::google::storage::v2::RewriteObjectRequest;
using ::google::storage::v2::RewriteResponse;

auto constexpr kResponseText = R"pb(
  total_bytes_rewritten: 3000
  object_size: 3000
  resource { size: 3000 })pb";

auto MakeRewriteResponse() {
  RewriteResponse response;
  EXPECT_TRUE(TextFormat::ParseFromString(kResponseText, &response));
  return response;
}

auto MatchRewriteResponse() { return IsProtoEqual(MakeRewriteResponse()); }

auto MatchRewriteRequest(char const* expected) {
  RewriteObjectRequest request;
  EXPECT_TRUE(TextFormat::ParseFromString(expected, &request));
  return IsProtoEqual(std::move(request));
}

TEST(AsyncClient, StartRewrite1) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([](AsyncConnection::RewriteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          source_bucket: "projects/_/buckets/src-bucket"
          source_object: "src-object"
          destination_bucket: "projects/_/buckets/dst-bucket"
          destination_name: "dst-object"
        )pb";
        EXPECT_THAT(p.request, MatchRewriteRequest(kExpected));
        auto rewriter = std::make_shared<MockAsyncRewriterConnection>();
        EXPECT_CALL(*rewriter, Iterate).WillOnce([] {
          RewriteResponse response;
          response.set_total_bytes_rewritten(3000);
          response.set_object_size(3000);
          response.mutable_resource()->set_size(3000);
          return make_ready_future(make_status_or(MakeRewriteResponse()));
        });
        return rewriter;
      });

  auto client = AsyncClient(mock);
  AsyncRewriter rewriter;
  AsyncToken token;
  std::tie(rewriter, token) =
      client.StartRewrite(BucketName("src-bucket"), "src-object",
                          BucketName("dst-bucket"), "dst-object",
                          Options{}
                              .set<TestOption<1>>("O1-function")
                              .set<TestOption<2>>("O2-function"));
  auto rt = rewriter.Iterate(std::move(token)).get();
  ASSERT_STATUS_OK(rt);
  EXPECT_FALSE(rt->second.valid());
  EXPECT_THAT(rt->first, MatchRewriteResponse());
}

TEST(AsyncClient, StartRewrite2) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([](AsyncConnection::RewriteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          source_bucket: "src-invalid-test-only"
          source_object: "src-object"
          destination_bucket: "dst-invalid-test-only"
          destination_name: "dst-object"
          if_generation_match: 42
          max_bytes_rewritten_per_call: 12345
        )pb";
        EXPECT_THAT(p.request, MatchRewriteRequest(kExpected));
        auto rewriter = std::make_shared<MockAsyncRewriterConnection>();
        EXPECT_CALL(*rewriter, Iterate).WillOnce([] {
          RewriteResponse response;
          response.set_total_bytes_rewritten(3000);
          response.set_object_size(3000);
          response.mutable_resource()->set_size(3000);
          return make_ready_future(make_status_or(MakeRewriteResponse()));
        });
        return rewriter;
      });

  auto client = AsyncClient(mock);
  AsyncRewriter rewriter;
  AsyncToken token;
  RewriteObjectRequest request;
  request.set_source_bucket("src-invalid-test-only");
  request.set_source_object("src-object");
  request.set_destination_bucket("dst-invalid-test-only");
  request.set_destination_name("dst-object");
  request.set_if_generation_match(42);
  request.set_max_bytes_rewritten_per_call(12345);
  std::tie(rewriter, token) = client.StartRewrite(
      std::move(request), Options{}
                              .set<TestOption<1>>("O1-function")
                              .set<TestOption<2>>("O2-function"));
  auto rt = rewriter.Iterate(std::move(token)).get();
  ASSERT_STATUS_OK(rt);
  EXPECT_FALSE(rt->second.valid());
  EXPECT_THAT(rt->first, MatchRewriteResponse());
}

TEST(AsyncClient, ResumeRewrite1) {
  using ::google::storage::v2::RewriteObjectRequest;
  using ::google::storage::v2::RewriteResponse;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([](AsyncConnection::RewriteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          source_bucket: "projects/_/buckets/src-bucket"
          source_object: "src-object"
          destination_bucket: "projects/_/buckets/dst-bucket"
          destination_name: "dst-object"
          rewrite_token: "test-rewrite-token"
        )pb";
        EXPECT_THAT(p.request, MatchRewriteRequest(kExpected));
        auto rewriter = std::make_shared<MockAsyncRewriterConnection>();
        EXPECT_CALL(*rewriter, Iterate).WillOnce([] {
          return make_ready_future(make_status_or(MakeRewriteResponse()));
        });
        return rewriter;
      });

  auto client = AsyncClient(mock);
  AsyncRewriter rewriter;
  AsyncToken token;
  std::tie(rewriter, token) = client.ResumeRewrite(
      BucketName("src-bucket"), "src-object", BucketName("dst-bucket"),
      "dst-object", "test-rewrite-token",
      Options{}
          .set<TestOption<1>>("O1-function")
          .set<TestOption<2>>("O2-function"));
  auto rt = rewriter.Iterate(std::move(token)).get();
  ASSERT_STATUS_OK(rt);
  EXPECT_FALSE(rt->second.valid());
  EXPECT_THAT(rt->first, MatchRewriteResponse());
}

TEST(AsyncClient, ResumeRewrite2) {
  using ::google::storage::v2::RewriteObjectRequest;
  using ::google::storage::v2::RewriteResponse;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(
          Return(Options{}.set<TestOption<0>>("O0").set<TestOption<1>>("O1")));

  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([](AsyncConnection::RewriteObjectParams const& p) {
        EXPECT_THAT(p.options.get<TestOption<0>>(), "O0");
        EXPECT_THAT(p.options.get<TestOption<1>>(), "O1-function");
        EXPECT_THAT(p.options.get<TestOption<2>>(), "O2-function");
        auto constexpr kExpected = R"pb(
          source_bucket: "src-invalid-test-only"
          source_object: "src-object"
          destination_bucket: "dst-invalid-test-only"
          destination_name: "dst-object"
          rewrite_token: "test-rewrite-token"
          max_bytes_rewritten_per_call: 12345
        )pb";
        EXPECT_THAT(p.request, MatchRewriteRequest(kExpected));
        auto rewriter = std::make_shared<MockAsyncRewriterConnection>();
        EXPECT_CALL(*rewriter, Iterate).WillOnce([] {
          return make_ready_future(make_status_or(MakeRewriteResponse()));
        });
        return rewriter;
      });

  auto client = AsyncClient(mock);
  AsyncRewriter rewriter;
  AsyncToken token;
  RewriteObjectRequest request;
  request.set_source_bucket("src-invalid-test-only");
  request.set_source_object("src-object");
  request.set_destination_bucket("dst-invalid-test-only");
  request.set_destination_name("dst-object");
  request.set_rewrite_token("test-rewrite-token");
  request.set_max_bytes_rewritten_per_call(12345);
  std::tie(rewriter, token) = client.ResumeRewrite(
      std::move(request), Options{}
                              .set<TestOption<1>>("O1-function")
                              .set<TestOption<2>>("O2-function"));
  auto rt = rewriter.Iterate(std::move(token)).get();
  ASSERT_STATUS_OK(rt);
  EXPECT_FALSE(rt->second.valid());
  EXPECT_THAT(rt->first, MatchRewriteResponse());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
