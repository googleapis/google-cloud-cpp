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
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockAsyncObjectMediaStream;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::VariantWith;

using AsyncReadObjectStream = ::google::cloud::internal::AsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>;

struct HashTestCase {
  StatusCode expected_status_code;
  Options options;
  absl::optional<std::int32_t> generated_crc32c;
  std::string generated_md5;
};

auto BinaryMD5(std::string const& md5) {
  auto binary = google::cloud::internal::HexDecode(md5);
  return std::string{binary.begin(), binary.end()};
}

auto GeneratedObjectChecksums(HashTestCase const& tc) {
  auto checksums = google::storage::v2::ObjectChecksums{};
  if (tc.generated_crc32c.has_value()) {
    checksums.set_crc32c(*tc.generated_crc32c);
  }
  if (!tc.generated_md5.empty()) {
    checksums.set_md5_hash(BinaryMD5(tc.generated_md5));
  }
  return checksums;
}

std::ostream& operator<<(std::ostream& os, HashTestCase const& rhs) {
  os << "HashTestCase={options={";
  os << "expected_status_code=" << rhs.expected_status_code  //
     << std::boolalpha                                       //
     << ", enable_crc32c_validation="
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
  os << "}, generated={"
     << google::cloud::internal::DebugString(GeneratedObjectChecksums(rhs),
                                             google::cloud::TracingOptions());
  os << "}}";
  return os;
}

class AsyncConnectionImplReadHashTest
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
auto constexpr kQuickFoxCrc32cChecksumBad = 0x00000000;
auto constexpr kQuickFoxMD5Hash = "9e107d9d372bb6826bd81d3542a419d6";
auto constexpr kQuickFoxMD5HashBad = "00000000000000000000000000000000";
auto constexpr kQuickFox = "The quick brown fox jumps over the lazy dog";

INSTANTIATE_TEST_SUITE_P(
    Computed, AsyncConnectionImplReadHashTest,
    ::testing::Values(
        HashTestCase{
            // This is the common case. Only CRC32C is enabled by default. The
            // service returns both CRC32C and MD5 values.
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        HashTestCase{
            // This is also common, the service does not return a MD5 value.
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksum, ""},
        // Make sure things work when both hashes are validated too.
        HashTestCase{
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        // In the next three cases we verify that disabling some validation
        // works.
        HashTestCase{
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5HashBad},
        HashTestCase{
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(false)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            kQuickFoxCrc32cChecksumBad, kQuickFoxMD5Hash},
        HashTestCase{
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(false)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksumBad, kQuickFoxMD5HashBad},
        // In the next three cases we verify that validation works when the
        // returned values are not correct.
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(false)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            kQuickFoxCrc32cChecksumBad, kQuickFoxMD5HashBad},
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(false),
            kQuickFoxCrc32cChecksumBad, kQuickFoxMD5HashBad},
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true),
            kQuickFoxCrc32cChecksumBad, kQuickFoxMD5HashBad},
        // The application may know what the values should be. Verify the
        // validation works correctly when the application provides correct
        // values.
        HashTestCase{
            StatusCode::kOk,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true)
                .set<storage_experimental::UseCrc32cValueOption>(
                    kQuickFoxCrc32cChecksum)
                .set<storage_experimental::UseMD5ValueOption>(
                    BinaryMD5(kQuickFoxMD5Hash)),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        // Verify bad values are detected
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true)
                .set<storage_experimental::UseCrc32cValueOption>(
                    kQuickFoxCrc32cChecksumBad)
                .set<storage_experimental::UseMD5ValueOption>(
                    BinaryMD5(kQuickFoxMD5Hash)),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true)
                .set<storage_experimental::UseCrc32cValueOption>(
                    kQuickFoxCrc32cChecksum)
                .set<storage_experimental::UseMD5ValueOption>(
                    BinaryMD5(kQuickFoxMD5HashBad)),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash},
        HashTestCase{
            StatusCode::kInvalidArgument,
            Options{}
                .set<storage_experimental::EnableCrc32cValidationOption>(true)
                .set<storage_experimental::EnableMD5ValidationOption>(true)
                .set<storage_experimental::UseCrc32cValueOption>(
                    kQuickFoxCrc32cChecksumBad)
                .set<storage_experimental::UseMD5ValueOption>(
                    BinaryMD5(kQuickFoxMD5HashBad)),
            kQuickFoxCrc32cChecksum, kQuickFoxMD5Hash}));

TEST_P(AsyncConnectionImplReadHashTest, ValidateFullChecksums) {
  auto const& param = GetParam();

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([&](auto) {
            auto response = google::storage::v2::ReadObjectResponse{};
            auto constexpr kObject = R"pb(
              bucket: "projects/_/buckets/test-bucket"
              name: "test-object"
              generation: 123456
            )pb";
            EXPECT_TRUE(TextFormat::ParseFromString(
                kObject, response.mutable_metadata()));
            SetMutableContent(*response.mutable_checksummed_data(),
                              ContentType(kQuickFox));
            response.mutable_checksummed_data()->set_crc32c(
                kQuickFoxCrc32cChecksum);
            *response.mutable_object_checksums() =
                GeneratedObjectChecksums(param);
            return absl::make_optional(response);
          });
        })
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            return absl::optional<google::storage::v2::ReadObjectResponse>{};
          });
        });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  });

  auto options =
      DefaultOptionsAsync(param.options)
          .set<GrpcNumChannelsOption>(1)
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(0));
  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection =
      MakeAsyncConnection(pool.cq(), std::move(mock), std::move(options));
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});

  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  // We expect the first `Read()` to return data, and the second to indicate the
  // end of the stream.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  EXPECT_THAT(data.get(), VariantWith<storage_experimental::ReadPayload>(_));

  // The last Read() triggers the end of stream message, including a call to
  // `Finish()`. It should detect the invalid checksum.
  data = reader->Read();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  // The stream Finish() function should be called in the background.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_THAT(data.get(),
              VariantWith<Status>(StatusIs(param.expected_status_code)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
