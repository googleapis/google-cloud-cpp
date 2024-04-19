// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <iomanip>
#include <iostream>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockAsyncBidiWriteObjectStream;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;

using AsyncBidiWriteObjectStream = ::google::cloud::AsyncStreamingReadWriteRpc<
    google::storage::v2::BidiWriteObjectRequest,
    google::storage::v2::BidiWriteObjectResponse>;

struct HashTestCase {
  Options options;
  absl::optional<std::int32_t> expected_crc32c;
  std::string expected_md5;
};

auto ExpectedObjectChecksums(HashTestCase const& tc) {
  auto expected_checksums = google::storage::v2::ObjectChecksums{};
  if (tc.expected_crc32c.has_value()) {
    expected_checksums.set_crc32c(*tc.expected_crc32c);
  }
  if (!tc.expected_md5.empty()) {
    auto binary = google::cloud::internal::HexDecode(tc.expected_md5);
    expected_checksums.set_md5_hash(std::string(binary.begin(), binary.end()));
  }
  return expected_checksums;
}

std::ostream& operator<<(std::ostream& os, HashTestCase const& rhs) {
  os << "HashTestCase={options={";
  os << std::boolalpha  //
     << "enable_crc32c_validation="
     << rhs.options.get<storage_experimental::EnableCrc32cValidationOption>();
  if (rhs.options.has<storage_experimental::UseCrc32cValueOption>()) {
    os << ", use_crc32_value="
       << rhs.options.get<storage_experimental::UseCrc32cValueOption>();
  }
  os << ", enable_md5_validation="
     << rhs.options.get<storage_experimental::EnableMD5ValidationOption>();
  if (rhs.options.has<storage_experimental::UseMD5ValueOption>()) {
    os << ", use_md5_value="
       << rhs.options.get<storage_experimental::UseMD5ValueOption>();
  }
  os << "}, expected={"
     << google::cloud::internal::DebugString(ExpectedObjectChecksums(rhs),
                                             google::cloud::TracingOptions());
  os << "}}";
  return os;
}

class AsyncConnectionImplUploadHashTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<HashTestCase> {};

// Use gsutil to obtain the CRC32C checksum (in base64):
//    TEXT="The quick brown fox jumps over the lazy dog"
//    /bin/echo -n $TEXT > /tmp/fox.txt
//    gsutil hash /tmp/fox.txt
// Hashes [base64] for /tmp/fox.txt:
//    Hash (crc32c): ImIEBA==
//    Hash (md5)   : nhB9nTcrtoJr2B01QqQZ1g==
//
// Then convert the base64 values to hex
//
//     echo "ImIEBA==" | openssl base64 -d | od -t x1
//     echo "nhB9nTcrtoJr2B01QqQZ1g==" | openssl base64 -d | od -t x1
//
// Which yields (in proto format):
//
//     CRC32C      : 0x22620404
//     MD5         : 9e107d9d372bb6826bd81d3542a419d6

auto constexpr kQuickFoxCrc32cChecksum = 0x22620404;
auto constexpr kQuickFoxMD5Hash = "9e107d9d372bb6826bd81d3542a419d6";
auto constexpr kQuickFox = "The quick brown fox jumps over the lazy dog";

INSTANTIATE_TEST_SUITE_P(
    Computed, AsyncConnectionImplUploadHashTest,
    ::testing::Values(
        HashTestCase{
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        HashTestCase{
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksum, ""},
        HashTestCase{
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(false)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            absl::nullopt, kQuickFoxMD5Hash},
        HashTestCase{
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(false)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            absl::nullopt, ""}));

TEST_P(AsyncConnectionImplUploadHashTest, StartUnbuffered) {
  auto const& param = GetParam();
  auto const expected_checksums = ExpectedObjectChecksums(param);

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite)
      .WillOnce([&](auto&, auto, auto,
                    google::storage::v2::StartResumableWriteRequest const&) {
        return sequencer.PushBack("StartResumableWrite(1)").then([](auto) {
          auto response = google::storage::v2::StartResumableWriteResponse{};
          response.set_upload_id("test-upload-id");
          return make_status_or(response);
        });
      });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "test-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(expected_checksums));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));

  auto pending = connection->StartUnbufferedUpload(
      {google::storage::v2::StartResumableWriteRequest{},
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_P(AsyncConnectionImplUploadHashTest,
       ResumeUnbufferedWithoutPersistedData) {
  auto const& param = GetParam();
  auto const expected_checksums = ExpectedObjectChecksums(param);

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "resume-upload-id");
            return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(0);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "resume-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(expected_checksums));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "resume-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_P(AsyncConnectionImplUploadHashTest, ResumeUnbufferedWithPersistedData) {
  auto const& param = GetParam();

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "resume-upload-id");
            return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(256 * 1024);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "resume-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(google::storage::v2::ObjectChecksums{}));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "resume-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 256 * 1024);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_P(AsyncConnectionImplUploadHashTest, StartBuffered) {
  auto const& param = GetParam();
  auto const expected_checksums = ExpectedObjectChecksums(param);

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite)
      .WillOnce([&](auto&, auto, auto,
                    google::storage::v2::StartResumableWriteRequest const&) {
        return sequencer.PushBack("StartResumableWrite(1)").then([](auto) {
          auto response = google::storage::v2::StartResumableWriteResponse{};
          response.set_upload_id("test-upload-id");
          return make_status_or(response);
        });
      });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "test-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(expected_checksums));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));

  auto pending = connection->StartBufferedUpload(
      {google::storage::v2::StartResumableWriteRequest{},
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_P(AsyncConnectionImplUploadHashTest, ResumeBufferedWithoutPersistedData) {
  auto const& param = GetParam();
  auto const expected_checksums = ExpectedObjectChecksums(param);

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "resume-upload-id");
            return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(0);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "resume-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(expected_checksums));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "resume-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_P(AsyncConnectionImplUploadHashTest, ResumeBufferedWithPersistedData) {
  auto const& param = GetParam();

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "resume-upload-id");
            return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(256 * 1024);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce(
            [&](google::storage::v2::BidiWriteObjectRequest const& request,
                grpc::WriteOptions wopt) {
              EXPECT_EQ(request.upload_id(), "resume-upload-id");
              EXPECT_TRUE(request.finish_write());
              EXPECT_THAT(request.object_checksums(),
                          IsProtoEqual(google::storage::v2::ObjectChecksums{}));
              EXPECT_TRUE(wopt.is_last_message());
              return sequencer.PushBack("Write");
            });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));

  auto connection = MakeAsyncConnection(CompletionQueue(mock_cq),
                                        std::move(mock), std::move(options));
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "resume-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 256 * 1024);

  auto w2 = writer->Finalize(storage_experimental::WritePayload(kQuickFox));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
