// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/reader_connection_resume.h"
#include "google/cloud/storage/async/resume_policy.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/async/reader_connection_impl.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/storage/testing/mock_hash_validator.h"
#include "google/cloud/storage/testing/mock_resume_policy.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::storage::testing::MockHashValidator;
using ::google::cloud::storage::testing::MockResumePolicy;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::storage_experimental::ResumePolicy;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockReader = ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

using MockAsyncReaderConnectionFactory = ::testing::MockFunction<future<
    StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>(
    storage::Generation, std::int64_t)>;

auto WithGeneration(std::int64_t expected) {
  return ::testing::ResultOf(
      "generation value is",
      [](storage::Generation const& g) { return g.value_or(0); },
      ::testing::Eq(expected));
}

auto WithoutGeneration() {
  return ::testing::ResultOf(
      "generation is not set",
      [](storage::Generation const& g) { return g.has_value(); },
      ::testing::Eq(false));
}

auto MakeTestObject() {
  auto constexpr kObject = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    name: "test-object"
    generation: 1234
    size: 4096
  )pb";
  auto object = google::storage::v2::Object{};
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kObject, &object));
  return object;
}

auto constexpr kReadSize = 500;
auto constexpr kRangeStart = 10000;

auto MakeMockReaderPartial(std::int64_t offset)
    -> std::unique_ptr<storage_experimental::AsyncReaderConnection> {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([offset] {
        auto payload = ReadPayload(std::string(kReadSize, '1'))
                           .set_metadata(MakeTestObject())
                           .set_offset(kRangeStart + offset);
        return make_ready_future(ReadResponse(std::move(payload)));
      })
      .WillOnce([offset] {
        auto payload = ReadPayload(std::string(kReadSize, '2'))
                           .set_offset(kRangeStart + offset + kReadSize);
        return make_ready_future(ReadResponse(std::move(payload)));
      })
      .WillOnce(
          [] { return make_ready_future(ReadResponse(TransientError())); });
  EXPECT_CALL(*mock, GetRequestMetadata).Times(0);
  return mock;
}

auto MakeMockReaderFull(int offset)
    -> std::unique_ptr<storage_experimental::AsyncReaderConnection> {
  auto mock = std::make_unique<MockReader>();
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Read).WillOnce([offset] {
      auto payload = ReadPayload(std::string(kReadSize, '3'))
                         .set_metadata(MakeTestObject())
                         .set_offset(kRangeStart + offset);
      return make_ready_future(ReadResponse(std::move(payload)));
    });
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(ReadResponse(Status{}));
    });
  }
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));
  return mock;
}

auto MakeMockReaderTransient()
    -> StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>> {
  return TransientError();
}

auto MakeMockReaderStartAndTransient()
    -> std::unique_ptr<storage_experimental::AsyncReaderConnection> {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(ReadResponse(TransientError()));
  });
  EXPECT_CALL(*mock, GetRequestMetadata).Times(0);
  return mock;
}

auto ContentsMatch(std::size_t size, char c) {
  return ResultOf(
      "contents match", [](ReadPayload const& p) { return p.contents(); },
      ElementsAre(Eq(std::string(size, c))));
}

auto IsInitialRead() {
  return VariantWith<ReadPayload>(
      AllOf(ContentsMatch(kReadSize, '1'),
            ResultOf(
                "metadata", [](ReadPayload const& p) { return p.metadata(); },
                Optional(IsProtoEqual(MakeTestObject()))),
            ResultOf(
                "offset", [](ReadPayload const& p) { return p.offset(); },
                kRangeStart)));
}

TEST(AsyncReaderConnectionResume, Resume) {
  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
    return make_ready_future(make_status_or(MakeMockReaderPartial(0)));
  });
  EXPECT_CALL(mock_factory, Call(WithGeneration(1234), 2 * kReadSize))
      .WillOnce([&] {
        return make_ready_future(
            make_status_or(MakeMockReaderPartial(2 * kReadSize)));
      })
      .WillOnce([&] {
        return make_ready_future(
            make_status_or(MakeMockReaderFull(4 * kReadSize)));
      });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(3);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillRepeatedly(Return(ResumePolicy::kContinue));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(), IsInitialRead());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '1')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '3')));

  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(IsOk()));

  auto const metadata = tested.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(AsyncReaderConnectionResume, HashValidation) {
  auto hash_function = std::make_shared<MockHashFunction>();
  // Normally the `AsyncReaderConnectionImpl` would call
  // `hash_function->Update()`. Here we are mocking the `*ConnectionImpl`, so
  // only `ProcessHashValues()` and `Finish()` get called.
  EXPECT_CALL(*hash_function, Finish)
      .WillOnce(Return(storage::internal::HashValues{"crc32c", "md5"}));

  auto hash_validator = std::make_unique<MockHashValidator>();
  EXPECT_CALL(*hash_validator, ProcessHashValues)
      .WillOnce([](storage::internal::HashValues const& hv) {
        EXPECT_EQ(hv.crc32c, "crc32c");
        EXPECT_EQ(hv.md5, "md5");
      })
      .WillOnce([](storage::internal::HashValues const& hv) {
        EXPECT_THAT(hv.crc32c, IsEmpty());
        EXPECT_THAT(hv.md5, IsEmpty());
      });
  EXPECT_CALL(std::move(*hash_validator), Finish)
      .WillOnce([](storage::internal::HashValues const&) {
        return storage::internal::HashValidator::Result{
            /*.received=*/{}, /*.computed=*/{}, /*.is_mismatch=*/false};
      });

  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([] {
          auto payload = ReadPayload(std::string(kReadSize, '1'));
          ReadPayloadImpl::SetObjectHashes(
              payload, storage::internal::HashValues{"crc32c", "md5"});
          return make_ready_future(ReadResponse(std::move(payload)));
        })
        .WillOnce([] {
          auto payload = ReadPayload(std::string(kReadSize, '2'));
          return make_ready_future(ReadResponse(std::move(payload)));
        })
        .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });
    return make_ready_future(make_status_or(
        std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            std::move(mock))));
  });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(1);
  EXPECT_CALL(*resume_policy, OnFinish).Times(0);

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), std::move(hash_function),
      std::move(hash_validator), mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '1')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(IsOk()));
}

TEST(AsyncReaderConnectionResume, HashValidationWithError) {
  auto hash_function = std::make_shared<MockHashFunction>();
  // Normally the `AsyncReaderConnectionImpl` would call
  // `hash_function->Update()`. Here we are mocking the `*ConnectionImpl`, so
  // only `ProcessHashValues()` and `Finish()` get called.
  EXPECT_CALL(*hash_function, Finish)
      .WillOnce(Return(storage::internal::HashValues{"crc32c", "md5"}));

  auto hash_validator = std::make_unique<MockHashValidator>();
  EXPECT_CALL(*hash_validator, ProcessHashValues)
      .WillOnce([](storage::internal::HashValues const& hv) {
        EXPECT_EQ(hv.crc32c, "crc32c");
        EXPECT_EQ(hv.md5, "md5");
      })
      .WillOnce([](storage::internal::HashValues const& hv) {
        EXPECT_THAT(hv.crc32c, IsEmpty());
        EXPECT_THAT(hv.md5, IsEmpty());
      });
  EXPECT_CALL(std::move(*hash_validator), Finish)
      .WillOnce([](storage::internal::HashValues const&) {
        return storage::internal::HashValidator::Result{
            /*.received=*/{"crc32c", "md5"},
            /*.computed=*/{"crc32c-computed", "md5-computed"},
            /*.is_mismatch=*/true};
      });

  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([] {
          auto payload = ReadPayload(std::string(kReadSize, '1'));
          ReadPayloadImpl::SetObjectHashes(
              payload, storage::internal::HashValues{"crc32c", "md5"});
          return make_ready_future(ReadResponse(std::move(payload)));
        })
        .WillOnce([] {
          auto payload = ReadPayload(std::string(kReadSize, '2'));
          return make_ready_future(ReadResponse(std::move(payload)));
        })
        .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });
    return make_ready_future(make_status_or(
        std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            std::move(mock))));
  });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(1);
  EXPECT_CALL(*resume_policy, OnFinish).Times(0);

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), std::move(hash_function),
      std::move(hash_validator), mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '1')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));
}

TEST(AsyncReaderConnectionResume, Cancel) {
  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Cancel).Times(1);
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(ReadResponse(TransientError()));
    });
    return make_ready_future(make_status_or(
        std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            std::move(mock))));
  });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(1);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillRepeatedly(Return(ResumePolicy::kStop));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  EXPECT_NO_FATAL_FAILURE(tested.Cancel());
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(TransientError()));
  EXPECT_NO_FATAL_FAILURE(tested.Cancel());
}

TEST(AsyncReaderConnectionResume, GetRequestMetadata) {
  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(ReadResponse(TransientError()));
    });
    EXPECT_CALL(*mock, GetRequestMetadata)
        .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                     {{"tk0", "v0"}, {"tk1", "v1"}}}));
    return make_ready_future(make_status_or(
        std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            std::move(mock))));
  });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(1);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillRepeatedly(Return(ResumePolicy::kStop));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  auto const m0 = tested.GetRequestMetadata();
  EXPECT_THAT(m0.headers, IsEmpty());
  EXPECT_THAT(m0.trailers, IsEmpty());
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(TransientError()));
  auto const m1 = tested.GetRequestMetadata();
  EXPECT_THAT(m1.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(m1.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(AsyncReaderConnectionResume, ResumeUpdatesOffset) {
  MockAsyncReaderConnectionFactory mock_factory;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([] {
      return make_ready_future(make_status_or(MakeMockReaderPartial(0)));
    });
    EXPECT_CALL(mock_factory, Call(WithGeneration(1234), 2 * kReadSize))
        .WillOnce([] {
          return make_ready_future(
              make_status_or(MakeMockReaderStartAndTransient()));
        });
    EXPECT_CALL(mock_factory, Call(WithGeneration(1234), 0)).WillOnce([] {
      return make_ready_future(
          make_status_or(MakeMockReaderStartAndTransient()));
    });
    EXPECT_CALL(mock_factory, Call(WithGeneration(1234), 0)).WillOnce([] {
      return make_ready_future(
          make_status_or(MakeMockReaderFull(2 * kReadSize)));
    });
  }

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(4);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillRepeatedly(Return(ResumePolicy::kContinue));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '1')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '3')));
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(IsOk()));

  auto const metadata = tested.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

/// @test Verify the connection does *not* resume if there is an error.
TEST(AsyncReaderConnectionResume, StopOnReconnectError) {
  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithGeneration(0), 0)).WillOnce([&] {
    return make_ready_future(make_status_or(MakeMockReaderPartial(0)));
  });
  EXPECT_CALL(mock_factory, Call(WithGeneration(1234), 2 * kReadSize))
      .WillOnce([&] { return make_ready_future(MakeMockReaderTransient()); });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(1);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillRepeatedly(Return(ResumePolicy::kContinue));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(), IsInitialRead());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(TransientError()));
}

/// @test Verify the connection does *not* resume if the download is interrupted
/// too many times.
TEST(AsyncReaderConnectionResume, StopAfterTooManyReconnects) {
  MockAsyncReaderConnectionFactory mock_factory;
  EXPECT_CALL(mock_factory, Call(WithoutGeneration(), 0)).WillOnce([] {
    return make_ready_future(make_status_or(MakeMockReaderPartial(0)));
  });

  EXPECT_CALL(mock_factory, Call(WithGeneration(1234), _)).WillRepeatedly([] {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Read).WillOnce([] {
      return make_ready_future(ReadResponse(TransientError()));
    });
    EXPECT_CALL(*mock, GetRequestMetadata).Times(AtMost(1));
    return make_ready_future(make_status_or(
        std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            std::move(mock))));
  });

  auto resume_policy = std::make_unique<MockResumePolicy>();
  EXPECT_CALL(*resume_policy, OnStartSuccess).Times(3);
  EXPECT_CALL(*resume_policy, OnFinish)
      .WillOnce(Return(ResumePolicy::kContinue))
      .WillOnce(Return(ResumePolicy::kContinue))
      .WillOnce(Return(ResumePolicy::kStop));

  AsyncReaderConnectionResume tested(
      std::move(resume_policy), storage::internal::CreateNullHashFunction(),
      storage::internal::CreateNullHashValidator(),
      mock_factory.AsStdFunction());
  EXPECT_THAT(tested.Read().get(), IsInitialRead());
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ContentsMatch(kReadSize, '2')));

  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(TransientError()));
  auto const metadata = tested.GetRequestMetadata();
  EXPECT_THAT(metadata.headers, IsEmpty());
  EXPECT_THAT(metadata.trailers, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
