// Copyright 2022 Google LLC
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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/async/idempotency_policy.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/async/read_all.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Not;
using ::testing::Optional;
using ::testing::VariantWith;

class AsyncClientIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
    ASSERT_THAT(bucket_name_, Not(IsEmpty()))
        << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";
  }

  std::string const& bucket_name() const { return bucket_name_; }

  using google::cloud::storage::testing::StorageIntegrationTest::
      ScheduleForDelete;
  void ScheduleForDelete(google::storage::v2::Object const& object) {
    ScheduleForDelete(storage::ObjectMetadata{}
                          .set_bucket(MakeBucketName(object.bucket())->name())
                          .set_name(object.name())
                          .set_generation(object.generation()));
  }

 private:
  std::string bucket_name_;
};

auto TestOptions() {
  // Disable metrics in the test, they just make the logs harder to grok.
  return Options{}
      .set<storage_experimental::EnableGrpcMetricsOption>(false)
      .set<GrpcNumChannelsOption>(1)
      .set<GrpcTracingOptionsOption>(TracingOptions().SetOptions(
          "truncate_string_field_longer_than=2048"));
}

auto AlwaysRetry() {
  return TestOptions().set<IdempotencyPolicyOption>(
      MakeAlwaysRetryIdempotencyPolicy);
}

TEST_F(AsyncClientIntegrationTest, ObjectCRUD) {
  auto async = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();

  auto insert = async
                    .InsertObject(BucketName(bucket_name()), object_name,
                                  LoremIpsum(), AlwaysRetry())
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto full0 = async.ReadObjectRange(BucketName(bucket_name()), object_name, 0,
                                     LoremIpsum().size());
  auto full1 = async.ReadObjectRange(BucketName(bucket_name()), object_name, 0,
                                     LoremIpsum().size());
  auto partial0 = async.ReadObjectRange(BucketName(bucket_name()), object_name,
                                        2, LoremIpsum().size());
  auto partial1 = async.ReadObjectRange(BucketName(bucket_name()), object_name,
                                        2, LoremIpsum().size());

  for (auto* p : {&full1, &full0}) {
    auto response = p->get();
    ASSERT_STATUS_OK(response);
    auto contents = response->contents();
    auto const full = std::accumulate(contents.begin(), contents.end(),
                                      std::string{}, [](auto a, auto b) {
                                        a += std::string(b);
                                        return a;
                                      });
    EXPECT_EQ(full, LoremIpsum());
  }
  for (auto* p : {&partial1, &partial0}) {
    auto response = p->get();
    ASSERT_STATUS_OK(response);
    auto contents = response->contents();
    auto const partial = std::accumulate(contents.begin(), contents.end(),
                                         std::string{}, [](auto a, auto b) {
                                           a += std::string(b);
                                           return a;
                                         });
    EXPECT_EQ(partial, LoremIpsum().substr(2));
  }

  auto status = async
                    .DeleteObject(BucketName(bucket_name()), object_name,
                                  insert->generation())
                    .get();
  EXPECT_STATUS_OK(status);

  auto request = google::storage::v2::ReadObjectRequest{};
  request.set_bucket(insert->bucket());
  request.set_object(insert->name());
  request.set_generation(insert->generation());
  auto head = async.ReadObjectRange(request, /*offset=*/0, /*limit=*/1).get();
  EXPECT_THAT(head, StatusIs(StatusCode::kNotFound));
}

TEST_F(AsyncClientIntegrationTest, ComposeObject) {
  auto async = AsyncClient(TestOptions());
  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();
  auto destination = MakeRandomObjectName();

  auto insert1 = async.InsertObject(BucketName(bucket_name()), o1, LoremIpsum(),
                                    AlwaysRetry());
  auto insert2 = async.InsertObject(BucketName(bucket_name()), o2, LoremIpsum(),
                                    AlwaysRetry());
  std::vector<StatusOr<google::storage::v2::Object>> inserted{insert1.get(),
                                                              insert2.get()};
  for (auto const& insert : inserted) {
    ASSERT_STATUS_OK(insert);
    ScheduleForDelete(*insert);
  }
  std::vector<google::storage::v2::ComposeObjectRequest::SourceObject> sources;
  std::transform(inserted.begin(), inserted.end(), std::back_inserter(sources),
                 [](auto const& o) {
                   google::storage::v2::ComposeObjectRequest::SourceObject r;
                   r.set_name(o->name());
                   r.set_generation(o->generation());
                   return r;
                 });
  auto pending = async.ComposeObject(BucketName(bucket_name()), destination,
                                     std::move(sources));
  auto const composed = pending.get();
  EXPECT_STATUS_OK(composed);
  ScheduleForDelete(*composed);

  auto read = async
                  .ReadObjectRange(BucketName(bucket_name()), destination, 0,
                                   2 * LoremIpsum().size())
                  .get();
  ASSERT_STATUS_OK(read);
  auto contents = read->contents();
  auto const full_contents = std::accumulate(contents.begin(), contents.end(),
                                             std::string{}, [](auto a, auto b) {
                                               a += std::string(b);
                                               return a;
                                             });
  EXPECT_EQ(full_contents, LoremIpsum() + LoremIpsum());
  EXPECT_THAT(read->metadata(), Optional(IsProtoEqual(*composed)));
}

TEST_F(AsyncClientIntegrationTest, StreamingRead) {
  auto async = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a relatively large object so the streaming read makes sense. We
  // aim for something around 5MiB, enough for 3 `Read()` calls.
  auto constexpr kLineSize = 64;
  auto constexpr kLineCount = 5 * 1024 * 1024 / kLineSize;
  auto const block = MakeRandomData(kLineSize);
  std::vector<std::string> insert_data(kLineCount);
  std::generate(insert_data.begin(), insert_data.end(), [&, n = 0]() mutable {
    return std::to_string(++n) + ": " + block;
  });
  auto const expected_size = std::accumulate(
      insert_data.begin(), insert_data.end(), static_cast<std::size_t>(0),
      [](auto a, auto const& b) { return a + b.size(); });

  auto insert = async
                    .InsertObject(BucketName(bucket_name()), object_name,
                                  insert_data, AlwaysRetry())
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  ASSERT_EQ(insert->size(), expected_size);

  auto r = async.ReadObject(BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(r);
  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = *std::move(r);

  std::string actual;
  while (token.valid()) {
    auto p = reader.Read(std::move(token)).get();
    ASSERT_STATUS_OK(p);
    ReadPayload payload;
    std::tie(payload, token) = *std::move(p);
    for (auto v : payload.contents()) actual += std::string(v);
  }
  EXPECT_EQ(actual.size(), expected_size);
  auto view = absl::string_view(actual);
  for (auto const& expected : insert_data) {
    ASSERT_GE(view.size(), expected.size());
    ASSERT_EQ(expected, view.substr(0, expected.size()));
    view.remove_prefix(expected.size());
  }
  EXPECT_EQ(view, absl::string_view{});
}

TEST_F(AsyncClientIntegrationTest, StreamingReadRange) {
  auto async = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a relatively large object so the streaming read makes sense. We
  // aim for something around 5MiB, enough for 3 `Read()` calls.
  auto constexpr kLineSize = 64;
  auto constexpr kLineCount = 5 * 1024 * 1024 / kLineSize;
  auto constexpr kReadOffset = kLineCount * kLineSize / 2;
  auto const block = MakeRandomData(kLineSize - 1) + "\n";
  std::string contents;
  for (int i = 0; i != kLineCount; ++i) contents += block;
  auto const expected_insert_size = contents.size();

  auto insert = async
                    .InsertObject(BucketName(bucket_name()), object_name,
                                  contents, AlwaysRetry())
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  ASSERT_EQ(insert->size(), expected_insert_size);

  auto request = google::storage::v2::ReadObjectRequest{};
  request.set_bucket(insert->bucket());
  request.set_object(insert->name());
  request.set_generation(insert->generation());
  request.set_read_offset(kReadOffset);
  auto r = async.ReadObject(request).get();
  ASSERT_STATUS_OK(r);
  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = *std::move(r);

  std::string actual;
  while (token.valid()) {
    auto p = reader.Read(std::move(token)).get();
    ASSERT_STATUS_OK(p);
    ReadPayload payload;
    std::tie(payload, token) = *std::move(p);
    for (auto v : payload.contents()) actual += std::string(v);
  }

  EXPECT_EQ(absl::string_view(actual),
            absl::string_view(contents).substr(kReadOffset));
}

TEST_F(AsyncClientIntegrationTest, StartUnbufferedUploadEmpty) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();

  auto w = client.StartUnbufferedUpload(BucketName(bucket_name()), object_name)
               .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), 0);
}

TEST_F(AsyncClientIntegrationTest, StartUnbufferedUploadMultiple) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = 256 * 1024;
  auto constexpr kBlockCount = 16;
  auto const block = MakeRandomData(kBlockSize);

  auto w = client.StartUnbufferedUpload(BucketName(bucket_name()), object_name)
               .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i != kBlockCount; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kBlockCount * kBlockSize);
}

TEST_F(AsyncClientIntegrationTest, StartUnbufferedUploadResume) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = 256 * 1024;
  auto constexpr kInitialBlockCount = 4;
  auto constexpr kTotalBlockCount = 4 + kInitialBlockCount;
  auto constexpr kDesiredSize = kBlockSize * kTotalBlockCount;
  auto const block = MakeRandomData(kBlockSize);

  auto w = client.StartUnbufferedUpload(BucketName(bucket_name()), object_name)
               .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto const upload_id = writer.UploadId();
  for (int i = 0; i != kInitialBlockCount - 1; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  // Reset the existing writer and resume the upload.
  writer = AsyncWriter();
  w = client.ResumeUnbufferedUpload(upload_id).get();
  ASSERT_STATUS_OK(w);
  std::tie(writer, token) = *std::move(w);
  ASSERT_EQ(writer.UploadId(), upload_id);
  auto const persisted = writer.PersistedState();
  // We don't expect this to be larger that the total size of the object.
  // Incidentally, this shows the value fits into an `int`.
  ASSERT_THAT(persisted, VariantWith<std::int64_t>(Le(kDesiredSize)));
  // Cast to `int` because otherwise we need to write multiple casts below.
  auto offset = static_cast<int>(absl::get<std::int64_t>(persisted));
  if (offset % kBlockSize != 0) {
    auto s = block.substr(offset % kBlockSize);
    auto const size = s.size();
    auto p = writer.Write(std::move(token), WritePayload(std::move(s))).get();
    ASSERT_STATUS_OK(p);
    offset += static_cast<int>(size);
    token = *std::move(p);
  }
  while (offset < kDesiredSize) {
    auto const n = std::min(kBlockSize, kDesiredSize - offset);
    auto p =
        writer.Write(std::move(token), WritePayload(block.substr(0, n))).get();
    ASSERT_STATUS_OK(p);
    offset += n;
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kDesiredSize);
}

TEST_F(AsyncClientIntegrationTest, StartUnbufferedUploadResumeFinalized) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = static_cast<std::int64_t>(256 * 1024);
  auto const block = MakeRandomData(kBlockSize);

  auto w = client.StartUnbufferedUpload(BucketName(bucket_name()), object_name)
               .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto const upload_id = writer.UploadId();
  auto metadata = writer.Finalize(std::move(token), WritePayload(block)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kBlockSize);

  w = client.ResumeUnbufferedUpload(upload_id).get();
  ASSERT_STATUS_OK(w);
  std::tie(writer, token) = *std::move(w);
  EXPECT_FALSE(token.valid());
  EXPECT_THAT(writer.PersistedState(), VariantWith<google::storage::v2::Object>(
                                           IsProtoEqual(*metadata)));
}

TEST_F(AsyncClientIntegrationTest, StartBufferedUploadEmpty) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();

  auto w =
      client.StartBufferedUpload(BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), 0);
}

TEST_F(AsyncClientIntegrationTest, StartBufferedUploadMultiple) {
  auto client = AsyncClient(TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = 256 * 1024;
  auto constexpr kBlockCount = 16;
  auto const block = MakeRandomData(kBlockSize);

  auto w =
      client.StartBufferedUpload(BucketName(bucket_name()), object_name).get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i != kBlockCount; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kBlockCount * kBlockSize);
}

TEST_F(AsyncClientIntegrationTest, RewriteObject) {
  auto async = AsyncClient(TestOptions());
  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();

  auto constexpr kBlockSize = 4 * 1024 * 1024;
  auto insert = async
                    .InsertObject(BucketName(bucket_name()), o1,
                                  MakeRandomData(kBlockSize), AlwaysRetry())
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  // Start a rewrite, but limit each iteration to a small number of bytes, to
  // force multiple calls.
  google::storage::v2::Object metadata;
  AsyncRewriter rewriter;
  AsyncToken token;
  google::storage::v2::RewriteObjectRequest request;
  request.set_destination_name(o2);
  request.set_destination_bucket(BucketName(bucket_name()).FullName());
  request.set_source_object(o1);
  request.set_source_bucket(BucketName(bucket_name()).FullName());
  request.set_max_bytes_rewritten_per_call(1024 * 1024);
  std::tie(rewriter, token) = async.StartRewrite(std::move(request));
  while (token.valid()) {
    auto rt = rewriter.Iterate(std::move(token)).get();
    ASSERT_STATUS_OK(rt);
    google::storage::v2::RewriteResponse response;
    AsyncToken t;
    std::tie(response, t) = *std::move(rt);
    token = std::move(t);
    if (!response.has_resource()) continue;
    metadata = response.resource();
    ScheduleForDelete(metadata);
    EXPECT_FALSE(token.valid());
  }
  EXPECT_EQ(metadata.name(), o2);
  EXPECT_EQ(metadata.size(), insert->size());
}

TEST_F(AsyncClientIntegrationTest, RewriteObjectResume) {
  auto async = AsyncClient(TestOptions());
  auto destination =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME");
  if (!destination || destination->empty()) GTEST_SKIP();

  auto constexpr kBlockSize = 4 * 1024 * 1024;
  auto source =
      async
          .InsertObject(BucketName(bucket_name()), MakeRandomObjectName(),
                        MakeRandomData(kBlockSize), AlwaysRetry())
          .get();
  ASSERT_STATUS_OK(source);
  ScheduleForDelete(*source);

  // Start a rewrite, but limit each iteration to a small number of bytes, to
  // force multiple calls.
  AsyncRewriter rewriter;
  AsyncToken token;
  auto const expected_name = MakeRandomObjectName();
  google::storage::v2::RewriteObjectRequest start_request;
  start_request.set_destination_name(expected_name);
  start_request.set_destination_bucket(BucketName(*destination).FullName());
  start_request.set_source_object(source->name());
  start_request.set_source_bucket(source->bucket());
  start_request.set_max_bytes_rewritten_per_call(1024 * 1024);
  std::tie(rewriter, token) = async.StartRewrite(start_request);

  auto rt = rewriter.Iterate(std::move(token)).get();
  ASSERT_STATUS_OK(rt);
  google::storage::v2::RewriteResponse response;
  AsyncToken t;
  std::tie(response, t) = *std::move(rt);

  // We want to resume a partially completed resume. Verify the first rewrite
  // did not complete things.
  ASSERT_THAT(response.rewrite_token(), Not(IsEmpty()));

  google::storage::v2::RewriteObjectRequest resume_request;
  resume_request.set_source_bucket(source->bucket());
  resume_request.set_source_object(source->name());
  resume_request.set_destination_bucket(BucketName(*destination).FullName());
  resume_request.set_destination_name(expected_name);
  resume_request.set_max_bytes_rewritten_per_call(1024 * 1024);
  std::tie(rewriter, token) = async.ResumeRewrite(std::move(resume_request));

  google::storage::v2::Object metadata;
  while (token.valid()) {
    auto rt = rewriter.Iterate(std::move(token)).get();
    ASSERT_STATUS_OK(rt);
    AsyncToken t;
    std::tie(response, t) = *std::move(rt);
    token = std::move(t);
    if (!response.has_resource()) continue;
    metadata = response.resource();
    ScheduleForDelete(metadata);
    EXPECT_EQ(metadata.bucket(), BucketName(*destination).FullName());
    EXPECT_EQ(metadata.name(), expected_name);
    EXPECT_EQ(metadata.size(), source->size());
    EXPECT_FALSE(token.valid());
  }
}

TEST_F(AsyncClientIntegrationTest, InsertFailure) {
  auto async = AsyncClient(TestOptions());

  auto insert = async
                    .InsertObject(BucketName(MakeRandomBucketName()),
                                  MakeRandomObjectName(), LoremIpsum())
                    .get();
  ASSERT_THAT(insert, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ReadFailure) {
  auto async = AsyncClient(TestOptions());

  auto read = async
                  .ReadObject(BucketName(MakeRandomBucketName()),
                              MakeRandomObjectName())
                  .get();
  // At the moment, only connectivity errors are detected before the first
  // `Read()` call. Accept such failures too:
  if (!read) return;
  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = *std::move(read);
  auto payload = ReadAll(std::move(reader), std::move(token)).get();
  ASSERT_THAT(payload, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ReadRangeFailure) {
  auto async = AsyncClient(TestOptions());

  auto payload =
      async
          .ReadObjectRange(BucketName(MakeRandomBucketName()),
                           MakeRandomObjectName(), /*offset=*/0, /*limit=*/1)
          .get();
  ASSERT_THAT(payload, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, StartBufferedUploadFailure) {
  auto async = AsyncClient(TestOptions());

  auto writer = async
                    .StartBufferedUpload(BucketName(MakeRandomBucketName()),
                                         MakeRandomObjectName())
                    .get();
  ASSERT_THAT(writer, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ResumeBufferedUploadFailure) {
  auto async = AsyncClient(TestOptions());

  auto writer = async.ResumeBufferedUpload("test-only-invalid-upload-id").get();
  ASSERT_THAT(writer, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, StartUnbufferedUploadFailure) {
  auto async = AsyncClient(TestOptions());

  auto writer = async
                    .StartUnbufferedUpload(BucketName(MakeRandomBucketName()),
                                           MakeRandomObjectName())
                    .get();
  ASSERT_THAT(writer, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ResumeUnbufferedUploadFailure) {
  auto async = AsyncClient(TestOptions());

  auto writer =
      async.ResumeUnbufferedUpload("test-only-invalid-upload-id").get();
  ASSERT_THAT(writer, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ComposeObjectFailure) {
  auto async = AsyncClient(TestOptions());

  auto make_source = [](std::string name) {
    auto source = google::storage::v2::ComposeObjectRequest::SourceObject{};
    source.set_name(std::move(name));
    return source;
  };
  auto composed =
      async
          .ComposeObject(BucketName(bucket_name()), MakeRandomObjectName(),
                         {make_source(MakeRandomObjectName()),
                          make_source(MakeRandomObjectName())})
          .get();
  ASSERT_THAT(composed, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, DeleteObjectFailure) {
  auto async = AsyncClient(TestOptions());

  auto deleted =
      async.DeleteObject(BucketName(bucket_name()), MakeRandomObjectName())
          .get();
  ASSERT_THAT(deleted, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, StartRewriteFailure) {
  auto async = AsyncClient(TestOptions());

  AsyncRewriter rewriter;
  AsyncToken token;
  std::tie(rewriter, token) =
      async.StartRewrite(BucketName(bucket_name()), MakeRandomObjectName(),
                         BucketName(bucket_name()), MakeRandomObjectName());
  ASSERT_TRUE(token.valid());
  auto iteration = rewriter.Iterate(std::move(token)).get();
  ASSERT_THAT(iteration, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, ResumeRewriteFailure) {
  auto async = AsyncClient(TestOptions());

  AsyncRewriter rewriter;
  AsyncToken token;
  std::tie(rewriter, token) =
      async.ResumeRewrite(BucketName(bucket_name()), MakeRandomObjectName(),
                          BucketName(bucket_name()), MakeRandomObjectName(),
                          "test-only-invalid-rewrite-token");
  ASSERT_TRUE(token.valid());
  auto iteration = rewriter.Iterate(std::move(token)).get();
  ASSERT_THAT(iteration, Not(IsOk()));
}

TEST_F(AsyncClientIntegrationTest, StartAppendableObjectUploadEmpty) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto async = AsyncClient(TestOptions());
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();

  auto create =
      client.CreateBucket(bucket_name(), storage::BucketMetadata{}
                                             .set_location("us-west4")
                                             .set_storage_class("RAPID"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }
  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), 0);
}

TEST_F(AsyncClientIntegrationTest, StartAppendableObjectUploadMultiple) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto async = AsyncClient(TestOptions());
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = 256 * 1024;
  auto constexpr kBlockCount = 16;
  auto const block = MakeRandomData(kBlockSize);

  auto create =
      client.CreateBucket(bucket_name(), storage::BucketMetadata{}
                                             .set_location("us-west4")
                                             .set_storage_class("RAPID"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }
  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i != kBlockCount; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kBlockCount * kBlockSize);
}

TEST_F(AsyncClientIntegrationTest, ResumeAppendableObjectUpload) {
  // Skipping the test till we get the takeover feature on testbench.
  GTEST_SKIP();
  auto async = AsyncClient(TestOptions());
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = 256 * 1024;
  auto constexpr kInitialBlockCount = 4;
  auto constexpr kTotalBlockCount = 4 + kInitialBlockCount;
  auto constexpr kDesiredSize = kBlockSize * kTotalBlockCount;
  auto const block = MakeRandomData(kBlockSize);

  auto create =
      client.CreateBucket(bucket_name(), storage::BucketMetadata{}
                                             .set_location("us-west4")
                                             .set_storage_class("RAPID"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }
  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  for (int i = 0; i != kInitialBlockCount - 1; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  writer.Close();

  // Reset the existing writer and resume the upload.
  writer = AsyncWriter();

  auto object_metadata = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(object_metadata);
  auto m = *object_metadata;
  auto generation = m.generation();

  w = async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation)
          .get();
  ASSERT_STATUS_OK(w);
  std::tie(writer, token) = *std::move(w);
  auto const persisted = writer.PersistedState();
  ASSERT_THAT(persisted, VariantWith<std::int64_t>(Le(kDesiredSize)));
  // Cast to `int` because otherwise we need to write multiple casts below.
  auto offset = static_cast<int>(absl::get<std::int64_t>(persisted));
  if (offset % kBlockSize != 0) {
    auto s = block.substr(offset % kBlockSize);
    auto const size = s.size();
    auto p = writer.Write(std::move(token), WritePayload(std::move(s))).get();
    ASSERT_STATUS_OK(p);
    offset += static_cast<int>(size);
    token = *std::move(p);
  }
  while (offset < kDesiredSize) {
    auto const n = std::min(kBlockSize, kDesiredSize - offset);
    auto p =
        writer.Write(std::move(token), WritePayload(block.substr(0, n))).get();
    ASSERT_STATUS_OK(p);
    offset += n;
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kDesiredSize);
}

TEST_F(AsyncClientIntegrationTest, ResumeFinalizedAppendableObjectUpload) {
  // Skipping the test till we get the takeover feature on testbench.
  GTEST_SKIP();
  auto async = AsyncClient(TestOptions());
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();
  // Create a small block to send over and over.
  auto constexpr kBlockSize = static_cast<std::int64_t>(256 * 1024);
  auto const block = MakeRandomData(kBlockSize);

  auto create =
      client.CreateBucket(bucket_name(), storage::BucketMetadata{}
                                             .set_location("us-west4")
                                             .set_storage_class("RAPID"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }
  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);

  auto metadata = writer.Finalize(std::move(token), WritePayload(block)).get();
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(metadata->bucket(), BucketName(bucket_name()).FullName());
  EXPECT_EQ(metadata->name(), object_name);
  EXPECT_EQ(metadata->size(), kBlockSize);

  auto object_metadata = client.GetObjectMetadata(bucket_name(), object_name);
  ASSERT_STATUS_OK(object_metadata);
  auto m = *object_metadata;
  auto generation = m.generation();

  w = async
          .ResumeAppendableObjectUpload(BucketName(bucket_name()), object_name,
                                        generation)
          .get();
  ASSERT_STATUS_OK(w);
  std::tie(writer, token) = *std::move(w);
  EXPECT_FALSE(token.valid());
  EXPECT_THAT(writer.PersistedState(), VariantWith<google::storage::v2::Object>(
                                           IsProtoEqual(*metadata)));
}

TEST_F(AsyncClientIntegrationTest, Open) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto async = AsyncClient(TestOptions());
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();

  auto create = client.CreateBucket(
      bucket_name(), storage::BucketMetadata{}.set_location("us-west4"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }

  auto constexpr kSize = 8 * 1024;
  auto constexpr kStride = 2 * kSize;
  auto constexpr kBlockCount = 4;
  auto const block = MakeRandomData(kSize);

  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  for (int i = 0; i != kBlockCount; ++i) {
    auto p = writer.Write(std::move(token), WritePayload(block)).get();
    ASSERT_STATUS_OK(p);
    token = *std::move(p);
  }

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  spec.set_bucket(BucketName(bucket_name()).FullName());
  spec.set_object(object_name);
  auto descriptor = async.Open(spec).get();
  ASSERT_STATUS_OK(descriptor);

  AsyncReader r0;
  AsyncToken t0;
  auto actual0 = std::string{};
  std::tie(r0, t0) = descriptor->Read(0 * kStride, kSize);
  while (t0.valid()) {
    auto read = r0.Read(std::move(t0)).get();
    ASSERT_STATUS_OK(read);
    ReadPayload p;
    AsyncToken t;
    std::tie(p, t) = *std::move(read);
    for (auto sv : p.contents()) actual0 += std::string(sv);
    t0 = std::move(t);
  }

  EXPECT_EQ(actual0.size(), kSize);
  client.DeleteObject(bucket_name(), object_name,
                      storage::Generation(metadata->generation()));
}

TEST_F(AsyncClientIntegrationTest, OpenExceedMaximumRange) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto async = AsyncClient(
      TestOptions().set<storage_experimental::MaximumRangeSizeOption>(1024));
  auto client = MakeIntegrationTestClient(true, TestOptions());
  auto object_name = MakeRandomObjectName();

  auto create = client.CreateBucket(
      bucket_name(), storage::BucketMetadata{}.set_location("us-west4"));
  if (!create && create.status().code() != StatusCode::kAlreadyExists) {
    GTEST_FAIL() << "cannot create bucket: " << create.status();
  }

  auto constexpr kSize = 2048;
  auto const block = MakeRandomData(kSize);

  auto w =
      async.StartAppendableObjectUpload(BucketName(bucket_name()), object_name)
          .get();
  ASSERT_STATUS_OK(w);
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(w);
  auto p = writer.Write(std::move(token), WritePayload(block)).get();
  ASSERT_STATUS_OK(p);
  token = *std::move(p);

  auto metadata = writer.Finalize(std::move(token)).get();
  ASSERT_STATUS_OK(metadata);

  auto spec = google::storage::v2::BidiReadObjectSpec{};
  spec.set_bucket(BucketName(bucket_name()).FullName());
  spec.set_object(object_name);
  auto descriptor = async.Open(spec).get();
  ASSERT_STATUS_OK(descriptor);

  AsyncReader r0;
  AsyncToken t0;
  auto actual0 = std::string{};
  std::tie(r0, t0) = descriptor->Read(0, kSize);
  while (t0.valid()) {
    auto read = r0.Read(std::move(t0)).get();
    ASSERT_STATUS_OK(read);
    ReadPayload p;
    AsyncToken t;
    std::tie(p, t) = *std::move(read);
    for (auto sv : p.contents()) actual0 += std::string(sv);
    t0 = std::move(t);
  }

  EXPECT_EQ(actual0.size(), kSize);
  client.DeleteObject(bucket_name(), object_name,
                      storage::Generation(metadata->generation()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
