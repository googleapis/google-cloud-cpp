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

#include "google/cloud/storage/internal/async/reader_connection_impl.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/grpc/object_metadata_parser.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage::internal::HashValues;
using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockStream = google::cloud::testing_util::MockAsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>;
using ReadResponse =
    google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

google::cloud::internal::ImmutableOptions TestOptions() {
  return google::cloud::internal::MakeImmutableOptions(
      Options{}.set<storage::RestEndpointOption>(
          "https://test-only.p.googleapis.com"));
}

using HashUpdateType =
    std::conditional_t<std::is_same<absl::Cord, ContentType>::value,
                       absl::Cord const&, absl::string_view>;

TEST(ReaderConnectionImpl, CleanFinish) {
  auto constexpr kExpectedObject = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    name: "test-only-object-name"
    generation: 1234
    size: 4096
  )pb";
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([&] {
        google::storage::v2::ReadObjectResponse response;
        SetMutableContent(*response.mutable_checksummed_data(),
                          ContentType("test-only-1"));
        response.mutable_content_range()->set_start(1024);
        response.mutable_content_range()->set_end(2048);
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedObject,
                                                response.mutable_metadata()));
        return make_ready_future(absl::make_optional(response));
      })
      .WillOnce([] {
        google::storage::v2::ReadObjectResponse response;
        SetMutableContent(*response.mutable_checksummed_data(),
                          ContentType("test-only-2"));
        return make_ready_future(absl::make_optional(response));
      })
      .WillOnce([] {
        return make_ready_future(
            absl::optional<google::storage::v2::ReadObjectResponse>());
      });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status());
  });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));

  auto hash_function = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash_function, Update(_, An<HashUpdateType>(), _))
      .Times(2)
      .WillRepeatedly(Return(Status{}));

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock),
                                   std::move(hash_function));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(
                  AllOf(ResultOf(
                            "contents match",
                            [](storage_experimental::ReadPayload const& p) {
                              return p.contents();
                            },
                            ElementsAre("test-only-1")),
                        ResultOf(
                            "metadata.size",
                            [](storage_experimental::ReadPayload const& p) {
                              return p.metadata()
                                  .value_or(google::storage::v2::Object{})
                                  .size();
                            },
                            4096),
                        ResultOf(
                            "offset",
                            [](storage_experimental::ReadPayload const& p) {
                              return p.offset();
                            },
                            1024))));

  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ResultOf(
                  "contents match",
                  [](storage_experimental::ReadPayload const& p) {
                    return p.contents();
                  },
                  ElementsAre("test-only-2"))));

  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(IsOk()));

  auto const metadata = tested.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(ReaderConnectionImpl, WithError) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(
        absl::optional<google::storage::v2::ReadObjectResponse>());
  });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(PermanentError());
  });

  auto hash_function = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash_function, Update(_, An<HashUpdateType>(), _)).Times(0);

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock),
                                   std::move(hash_function));
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(PermanentError()));
}

TEST(ReaderConnectionImpl, HashingError) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(
        absl::make_optional(google::storage::v2::ReadObjectResponse{}));
  });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });

  auto hash_function = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash_function, Update(_, An<HashUpdateType>(), _))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "bad checksum")));

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock),
                                   std::move(hash_function));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));
}

TEST(ReaderConnectionImpl, FullHashes) {
  // /bin/echo -n 'The quick brown fox jumps over the lazy dog' > foo.txt
  // gsutil hash foo.txt
  auto constexpr kQuickFoxCrc32cChecksum = "ImIEBA==";
  auto constexpr kQuickFoxMD5Hash = "nhB9nTcrtoJr2B01QqQZ1g==";
  auto const crc32c = Crc32cToProto(kQuickFoxCrc32cChecksum).value();
  auto const md5 = MD5ToProto(kQuickFoxMD5Hash).value();

  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        google::storage::v2::ReadObjectResponse response;
        return make_ready_future(absl::make_optional(response));
      })
      .WillOnce([crc32c] {
        google::storage::v2::ReadObjectResponse response;
        response.mutable_object_checksums()->set_crc32c(crc32c);
        return make_ready_future(absl::make_optional(response));
      })
      .WillOnce([md5] {
        google::storage::v2::ReadObjectResponse response;
        response.mutable_object_checksums()->set_md5_hash(md5);
        return make_ready_future(absl::make_optional(response));
      })
      .WillOnce([crc32c, md5] {
        google::storage::v2::ReadObjectResponse response;
        response.mutable_object_checksums()->set_crc32c(crc32c);
        response.mutable_object_checksums()->set_md5_hash(md5);
        return make_ready_future(absl::make_optional(response));
      });

  auto hash_function = std::make_shared<MockHashFunction>();
  EXPECT_CALL(*hash_function, Update(_, An<HashUpdateType>(), _))
      .Times(4)
      .WillRepeatedly(Return(Status{}));

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock),
                                   std::move(hash_function));

  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(ResultOf(
                  "read payload without hash values",
                  [](storage_experimental::ReadPayload p) {
                    return ReadPayloadImpl::GetObjectHashes(p);
                  },
                  Eq(absl::nullopt))));

  auto has_hash_values = [](HashValues const& expected) {
    return ResultOf(
        "read response has hash values",
        [](storage_experimental::ReadPayload p) {
          return ReadPayloadImpl::GetObjectHashes(p);
        },
        AllOf(Optional(Field("crc32c", &HashValues::crc32c, expected.crc32c)),
              Optional(Field("md5", &HashValues::md5, expected.md5))));
  };

  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(
                  has_hash_values(HashValues{kQuickFoxCrc32cChecksum, {}})));
  EXPECT_THAT(tested.Read().get(), VariantWith<ReadPayload>(has_hash_values(
                                       HashValues{{}, kQuickFoxMD5Hash})));
  EXPECT_THAT(tested.Read().get(),
              VariantWith<ReadPayload>(has_hash_values(
                  HashValues{kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash})));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
