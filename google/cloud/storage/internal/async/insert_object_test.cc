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

#include "google/cloud/storage/internal/async/insert_object.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_async_streaming_write_rpc.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Request = google::storage::v2::WriteObjectRequest;
using Response = google::storage::v2::WriteObjectResponse;
using MockStream =
    ::google::cloud::testing_util::MockAsyncStreamingWriteRpc<Request,
                                                              Response>;

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;

struct StringOption {
  using Type = std::string;
};

auto constexpr kTestEndpoint = "https://test-only.p.gogleapis.com";
auto constexpr kExpectedProto = R"""(
    acl: {
      role: "OWNER"
      entity: "user:test-user@gmail.com"
      etag: "test-etag-acl"
    }
    content_encoding: "test-content-encoding"
    content_disposition: "test-content-disposition"
    cache_control: "test-cache-control"
    content_language: "test-content-language"
    metageneration: 42
    delete_time: {
      seconds: 1565194924
      nanos: 123456789
    }
    content_type: "test-content-type"
    size: 123456
    create_time: {
      seconds: 1565194924
      nanos: 234567890
    }
    # These magic numbers can be obtained using `gsutil hash` and then
    # transforming the output from base64 to binary using tools like xxd(1).
    checksums {
      crc32c: 576848900
      md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
    }
    component_count: 7
    update_time: {
      seconds: 1565194924
      nanos: 345678901
    }
    storage_class: "test-storage-class"
    kms_key: "test-kms-key-name"
    update_storage_class_time: {
      seconds: 1565194924
      nanos: 456789012
    }
    temporary_hold: true
    retention_expire_time: {
      seconds: 1565194924
      nanos: 567890123
    }
    metadata: { key: "test-key-1" value: "test-value-1" }
    metadata: { key: "test-key-2" value: "test-value-2" }
    event_based_hold: true
    name: "test-object-name"
    bucket: "test-bucket"
    generation: 2345
    owner: {
      entity: "test-entity"
      entity_id: "test-entity-id"

    }
    customer_encryption: {
      encryption_algorithm: "test-encryption-algorithm"
      key_sha256_bytes: "01234567"
    }
    etag: "test-etag"
)""";

Options TestOptions() {
  auto const* test_name =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  return Options{}
      .set<StringOption>(std::move(test_name))
      // Using a custom endpoint verifies that these options are used in the
      // conversion from protos to gcs::ObjectMetadata.
      .set<storage::RestEndpointOption>(kTestEndpoint);
}

auto MakeExpectedObject() {
  auto object = google::storage::v2::Object{};
  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedProto, &object));
  return object;
}

auto constexpr kExpectedChunkSize = 2 * 1024 * 1024L;

std::string RandomData(google::cloud::internal::DefaultPRNG& generator,
                       std::size_t size) {
  return google::cloud::internal::Sample(
      generator, static_cast<int>(size),
      "abcdefghijklmnopqrstuvwxyz0123456789");
}

TEST(InsertObject, SuccessEmpty) {
  AsyncSequencer<bool> sequencer;

  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&sequencer](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_TRUE(request.finish_write());
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.checksummed_data().crc32c(), 0);
        EXPECT_TRUE(wopt.is_last_message());
        return sequencer.PushBack("Write");
      });
  Response response;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kExpectedProto, response.mutable_resource()));
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [&](auto) { return make_status_or(response); });
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  google::storage::v2::WriteObjectRequest request;
  auto call =
      InsertObject::Call(std::move(rpc), std::move(hash), request, absl::Cord(),
                         internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  EXPECT_THAT(result.get(), IsOkAndHolds(IsProtoEqual(MakeExpectedObject())));
}

TEST(InsertObject, SuccessChunkAligned) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize);
  auto const view = absl::string_view(buffer.data(), buffer.size());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_TRUE(request.has_write_object_spec());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(0, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(kExpectedChunkSize)));
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.object_checksums().crc32c(), Crc32c(view));
        return sequencer.PushBack("Write");
      });
  Response response;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kExpectedProto, response.mutable_resource()));
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [&](auto) { return make_status_or(response); });
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.mutable_write_object_spec()->mutable_resource()->set_storage_class(
      "STANDARD");
  auto call = InsertObject::Call(std::move(rpc), std::move(hash), request,
                                 absl::Cord(buffer),
                                 internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  EXPECT_THAT(result.get(), IsOkAndHolds(IsProtoEqual(MakeExpectedObject())));
}

TEST(InsertObject, SuccessChunkPartial) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize + 1024);
  auto const view = absl::string_view(buffer.data(), buffer.size());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_TRUE(request.has_write_object_spec());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(0, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(kExpectedChunkSize, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 2 * kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(2 * kExpectedChunkSize)));
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.object_checksums().crc32c(), Crc32c(view));
        return sequencer.PushBack("Write");
      });
  Response response;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kExpectedProto, response.mutable_resource()));
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [&](auto) { return make_status_or(response); });
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.mutable_write_object_spec()->mutable_resource()->set_storage_class(
      "STANDARD");
  auto call = InsertObject::Call(std::move(rpc), std::move(hash), request,
                                 absl::Cord(buffer),
                                 internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  EXPECT_THAT(result.get(), IsOkAndHolds(IsProtoEqual(MakeExpectedObject())));
}

TEST(InsertObject, ErrorStart) {
  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [&](auto) { return StatusOr<Response>(PermanentError()); });
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.mutable_write_object_spec()->mutable_resource()->set_storage_class(
      "STANDARD");
  auto call =
      InsertObject::Call(std::move(rpc), std::move(hash), request, absl::Cord(),
                         internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto metadata = result.get();
  EXPECT_THAT(metadata,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(InsertObject, ErrorOnWrite) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Write).Times(2).WillRepeatedly([&sequencer] {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [&](auto) { return StatusOr<Response>(PermanentError()); });
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  google::storage::v2::WriteObjectRequest request;
  auto call = InsertObject::Call(
      std::move(rpc), std::move(hash), request,
      absl::Cord(RandomData(generator, 2 * kExpectedChunkSize)),
      internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto metadata = result.get();
  EXPECT_THAT(metadata,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(InsertObject, ErrorOnChecksums) {
  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Start).WillOnce([&sequencer] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*rpc, Cancel).WillOnce([&sequencer] {
    return sequencer.PushBack("Cancel");
  });
  EXPECT_CALL(*rpc, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then(
        [](auto) { return make_status_or(Response{}); });
  });

  auto hash = std::make_unique<storage::internal::PrecomputedHashFunction>(
      storage::internal::HashValues{"invalid", ""});
  google::storage::v2::WriteObjectRequest request;
  auto call =
      InsertObject::Call(std::move(rpc), std::move(hash), request, absl::Cord(),
                         internal::MakeImmutableOptions(TestOptions()));

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Cancel");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Finish");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto metadata = result.get();
  EXPECT_THAT(metadata, StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
