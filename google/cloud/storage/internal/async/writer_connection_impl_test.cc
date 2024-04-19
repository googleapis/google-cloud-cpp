// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/writer_connection_impl.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::WritePayload;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using Request = google::storage::v2::BidiWriteObjectRequest;
using Response = google::storage::v2::BidiWriteObjectResponse;
using MockStream =
    ::google::cloud::mocks::MockAsyncStreamingReadWriteRpc<Request, Response>;

auto TestOptions() {
  return google::cloud::internal::MakeImmutableOptions(
      Options{}.set<storage::RestEndpointOption>(
          "https://test-only.p.googleapis.com"));
}

auto MakeTestResponse() {
  Response response;
  response.mutable_resource()->set_size(2048);
  response.mutable_resource()->set_bucket("projects/_/buckets/test-bucket");
  response.mutable_resource()->set_name("test-object");
  return response;
}

auto MakeTestObject() {
  auto object = google::storage::v2::Object{};
  object.set_size(2048);
  object.set_bucket("projects/_/buckets/test-bucket");
  object.set_name("test-object");
  return object;
}

auto MakeRequest() {
  auto request = Request{};
  request.set_upload_id("test-upload-id");
  // We use this field as a canary to verify the request fields are preserved.
  request.mutable_common_object_request_params()->set_encryption_algorithm(
      "test-only-algo");
  return request;
}

TEST(AsyncWriterConnectionTest, Basic) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(0);
  EXPECT_CALL(*hash, Finish).Times(0);

  AsyncWriterConnectionImpl tested(TestOptions(), MakeRequest(),
                                   std::move(mock), hash, 1024);
  EXPECT_EQ(tested.UploadId(), "test-upload-id");
  EXPECT_THAT(tested.PersistedState(), VariantWith<std::int64_t>(1024));

  auto const metadata = tested.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(AsyncWriterConnectionTest, ResumeFinalized) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(0);
  EXPECT_CALL(*hash, Finish).Times(0);

  AsyncWriterConnectionImpl tested(TestOptions(), MakeRequest(),
                                   std::move(mock), hash, MakeTestObject());
  EXPECT_EQ(tested.UploadId(), "test-upload-id");
  EXPECT_THAT(tested.PersistedState(), VariantWith<google::storage::v2::Object>(
                                           IsProtoEqual(MakeTestObject())));
}

TEST(AsyncWriterConnectionTest, Cancel) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(2);
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(0);
  EXPECT_CALL(*hash, Finish).Times(0);

  AsyncWriterConnectionImpl tested(TestOptions(), MakeRequest(),
                                   std::move(mock), hash, 1024);
  tested.Cancel();
  EXPECT_EQ(tested.UploadId(), "test-upload-id");
  EXPECT_THAT(tested.PersistedState(), VariantWith<std::int64_t>(1024));
}

TEST(AsyncWriterConnectionTest, WriteSimple) {
  auto constexpr kChunk =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;
  auto constexpr kWriteCount = 4;
  auto constexpr kChunkCount = 5;
  std::int64_t offset = 3 * kChunk;

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write)
      .Times(kWriteCount * kChunkCount)
      .WillRepeatedly([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.write_offset(), offset);
        EXPECT_EQ(request.common_object_request_params().encryption_algorithm(),
                  "test-only-algo");
        EXPECT_FALSE(wopt.is_last_message());
        offset += kChunk;
        return sequencer.PushBack("Write");
      });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _))
      .Times(kWriteCount * kChunkCount);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, offset);

  for (int i = 0; i != kWriteCount; ++i) {
    auto response =
        tested->Write(WritePayload(std::string(kChunkCount * kChunk, 'A')));
    for (int j = 0; j != kChunkCount; ++j) {
      auto next = sequencer.PopFrontWithName();
      ASSERT_THAT(next.second, "Write");
      next.first.set_value(true);
    }
    EXPECT_STATUS_OK(response.get());
  }

  tested = {};
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);
}

TEST(AsyncWriterConnectionTest, WriteError) {
  auto constexpr kChunk =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 0);

  auto response = tested->Write(WritePayload(std::string(kChunk, 'A')));
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(false);  // Return an error from Finish()
  EXPECT_THAT(response.get(), StatusIs(PermanentError().code()));
}

TEST(AsyncWriterConnectionTest, UnexpectedWriteFailsWithoutError) {
  auto constexpr kChunk =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 0);

  auto response = tested->Write(WritePayload(std::string(kChunk, 'A')));
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncWriterConnectionTest, FinalizeEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(request.finish_write());
        EXPECT_TRUE(wopt.is_last_message());
        EXPECT_EQ(request.common_object_request_params().encryption_algorithm(),
                  "test-only-algo");
        return sequencer.PushBack("Write");
      });
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<Response>();
      return absl::make_optional(MakeTestResponse());
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(true);
  auto object = response.get();
  EXPECT_THAT(object, IsOkAndHolds(IsProtoEqual(MakeTestObject())))
      << "=" << object->DebugString();

  tested = {};
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);
}

TEST(AsyncWriterConnectionTest, FinalizeFails) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(false);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(PermanentError().code()));
}

TEST(AsyncWriterConnectionTest, UnexpectedFinalizeFailsWithoutError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncWriterConnectionTest, QueryFinalFails) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<Response>();
      return absl::make_optional(MakeTestResponse());
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(false);  // Detect an error during Read()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(false);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(PermanentError().code()));
}

TEST(AsyncWriterConnectionTest, UnexpectedQueryFinalFailsWithoutError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<Response>();
      return absl::make_optional(MakeTestResponse());
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(false);  // Detect an error during Read()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncWriterConnectionTest, UnexpectedQueryFinalMissingResource) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<Response>();
      return absl::make_optional(Response{});
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 0);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(true);
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kInternal));

  tested.reset();
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);
}

TEST(AsyncWriterConnectionTest, FlushEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(request.flush());
        EXPECT_TRUE(request.state_lookup());
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.common_object_request_params().encryption_algorithm(),
                  "test-only-algo");
        return sequencer.PushBack("Write");
      });
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<Response>();
      auto response = Response{};
      response.set_persisted_size(16384);
      return absl::make_optional(std::move(response));
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto flush = tested->Flush(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(true);
  EXPECT_THAT(flush.get(), IsOk());

  auto query = tested->Query();
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(true);
  EXPECT_THAT(query.get(), IsOkAndHolds(16384));

  tested = {};
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);
}

TEST(AsyncWriterConnectionTest, FlushFails) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(false);  // Return error from Finish()
  EXPECT_THAT(response.get(), StatusIs(PermanentError().code()));
}

TEST(AsyncWriterConnectionTest, UnexpectedFlushFailsWithoutError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Write).WillOnce([&](Request const&, grpc::WriteOptions) {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(1);
  EXPECT_CALL(*hash, Finish).Times(1);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto response = tested->Finalize(WritePayload{});
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Write");
  next.first.set_value(false);  // Detect an error on Write()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);  // Return success from Finish()
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kInternal));
}

TEST(AsyncWriterConnectionTest, QueryFails) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::optional<Response>(); });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(0);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto query = tested->Query();
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(false);  // Detect error from Read()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(false);  // Return error from Finish()
  EXPECT_THAT(query.get(), StatusIs(PermanentError().code()));
}

TEST(AsyncWriterConnectionTest, UnexpectedQueryFailsWithoutError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Read).WillOnce([&]() {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::optional<Response>(); });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return PermanentError();
    });
  });
  auto hash = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash, Update(_, An<absl::Cord const&>(), _)).Times(0);
  EXPECT_CALL(*hash, Finish).Times(0);

  auto tested = std::make_unique<AsyncWriterConnectionImpl>(
      TestOptions(), MakeRequest(), std::move(mock), hash, 1024);
  auto query = tested->Query();
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Read");
  next.first.set_value(false);  // Detect error from Read()
  next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, "Finish");
  next.first.set_value(true);  // Return success from Finish()
  EXPECT_THAT(query.get(), StatusIs(StatusCode::kInternal));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
