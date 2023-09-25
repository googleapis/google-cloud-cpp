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

#include "google/cloud/storage/async_client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <numeric>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace gcs = ::google::cloud::storage;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Not;

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

 private:
  std::string bucket_name_;
};

TEST_F(AsyncClientIntegrationTest, ObjectCRUD) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  auto async = AsyncClient();
  auto insert = async
                    .InsertObject(bucket_name(), object_name, LoremIpsum(),
                                  gcs::IfGenerationMatch(0))
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto pending0 =
      async.ReadObjectRange(bucket_name(), object_name, 0, LoremIpsum().size());
  auto pending1 =
      async.ReadObjectRange(bucket_name(), object_name, 0, LoremIpsum().size());

  for (auto* p : {&pending1, &pending0}) {
    auto response = p->get();
    EXPECT_STATUS_OK(response.status);
    auto const full = std::accumulate(response.contents.begin(),
                                      response.contents.end(), std::string{});
    EXPECT_EQ(full, LoremIpsum());
  }
  auto status = async
                    .DeleteObject(bucket_name(), object_name,
                                  gcs::Generation(insert->generation()))
                    .get();
  EXPECT_STATUS_OK(status);

  auto get = client->GetObjectMetadata(bucket_name(), object_name);
  EXPECT_THAT(get, StatusIs(StatusCode::kNotFound));
}

TEST_F(AsyncClientIntegrationTest, ComposeObject) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto o1 = MakeRandomObjectName();
  auto o2 = MakeRandomObjectName();
  auto destination = MakeRandomObjectName();

  auto async = AsyncClient();
  auto insert1 = async.InsertObject(bucket_name(), o1, LoremIpsum(),
                                    gcs::IfGenerationMatch(0));
  auto insert2 = async.InsertObject(bucket_name(), o2, LoremIpsum(),
                                    gcs::IfGenerationMatch(0));
  std::vector<StatusOr<storage::ObjectMetadata>> inserted{insert1.get(),
                                                          insert2.get()};
  for (auto const& insert : inserted) {
    ASSERT_STATUS_OK(insert);
    ScheduleForDelete(*insert);
  }
  std::vector<storage::ComposeSourceObject> sources(inserted.size());
  std::transform(inserted.begin(), inserted.end(), sources.begin(),
                 [](auto const& o) {
                   return storage::ComposeSourceObject{
                       o->name(), o->generation(), absl::nullopt};
                 });
  auto pending =
      async.ComposeObject(bucket_name(), std::move(sources), destination);
  auto const composed = pending.get();
  EXPECT_STATUS_OK(composed);
  ScheduleForDelete(*composed);

  auto read = async
                  .ReadObjectRange(bucket_name(), destination, 0,
                                   2 * LoremIpsum().size())
                  .get();
  ASSERT_STATUS_OK(read.status);
  auto const full_contents = std::accumulate(
      read.contents.begin(), read.contents.end(), std::string{});
  EXPECT_EQ(full_contents, LoremIpsum() + LoremIpsum());
  ASSERT_TRUE(read.object_metadata.has_value());
  EXPECT_EQ(*read.object_metadata, *composed);
}

TEST_F(AsyncClientIntegrationTest, StreamingRead) {
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

  auto async = AsyncClient();
  auto insert = async
                    .InsertObject(bucket_name(), object_name, insert_data,
                                  gcs::IfGenerationMatch(0))
                    .get();
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  ASSERT_EQ(insert->size(), expected_size);

  auto r = async.ReadObject(bucket_name(), object_name).get();
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
