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
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::testing_util::IsOk;
using ::testing::AllOf;
using ::testing::ElementsAre;
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

TEST(ReaderConnectionImpl, CleanFinish) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        google::storage::v2::ReadObjectResponse response;
        SetMutableContent(*response.mutable_checksummed_data(),
                          ContentType("test-only-1"));
        response.mutable_content_range()->set_start(1024);
        response.mutable_content_range()->set_end(2048);
        response.mutable_metadata()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_metadata()->set_name("test-only-object-name");
        response.mutable_metadata()->set_generation(1234);
        response.mutable_metadata()->set_size(4096);
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

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock));
  EXPECT_THAT(
      tested.Read().get(),
      VariantWith<ReadPayload>(AllOf(
          ResultOf(
              "contents match",
              [](storage_experimental::ReadPayload const& p) {
                return p.contents();
              },
              ElementsAre("test-only-1")),
          ResultOf(
              "metadata.size",
              [](storage_experimental::ReadPayload const& p) {
                return p.metadata().value_or(storage::ObjectMetadata()).size();
              },
              4096),
          ResultOf(
              "metadata.self_link",
              [](storage_experimental::ReadPayload const& p) {
                return p.metadata()
                    .value_or(storage::ObjectMetadata())
                    .self_link();
              },
              "https://test-only.p.googleapis.com/storage/v1/b/test-bucket/o/"
              "test-only-object-name"),
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

  AsyncReaderConnectionImpl tested(TestOptions(), std::move(mock));
  EXPECT_THAT(tested.Read().get(), VariantWith<Status>(PermanentError()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
