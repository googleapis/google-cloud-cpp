// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::StartsWith;

bool UsingEmulator() { return GetEnv("HTTPBIN_ENDPOINT").has_value(); }

std::string HttpBinEndpoint() { return GetEnv("HTTPBIN_ENDPOINT").value(); }

Status Make3Attempts(std::function<Status()> const& attempt) {
  Status status;
  auto backoff = std::chrono::seconds(1);
  for (int i = 0; i != 3; ++i) {
    status = attempt();
    if (status.ok()) return status;
    std::this_thread::sleep_for(backoff);
    backoff *= 2;
  }
  return status;
}

TEST(CurlDownloadRequestTest, SimpleStream) {
  if (!UsingEmulator()) GTEST_SKIP();
  // httpbin can generate up to 100 lines, do not try to download more than
  // that.
  constexpr int kDownloadedLines = 100;

  std::size_t count = 0;
  auto download = [&] {
    count = 0;
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/stream/" + std::to_string(kDownloadedLines),
        storage::internal::GetDefaultCurlHandleFactory());
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();
    char buffer[128 * 1024];
    do {
      auto n = sizeof(buffer);
      auto result = (*download)->Read(buffer, n);
      if (!result) return std::move(result).status();
      if (result->bytes_received > sizeof(buffer)) {
        return Status{StatusCode::kUnknown, "invalid byte count"};
      }
      count += static_cast<std::size_t>(
          std::count(buffer, buffer + result->bytes_received, '\n'));
      if (result->response.status_code != 100) break;
    } while (true);
    return Status{};
  };

  auto delay = std::chrono::seconds(1);
  for (int i = 0; i != 3; ++i) {
    auto result = download();
    if (result.ok()) break;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  EXPECT_EQ(kDownloadedLines, count);
}

TEST(CurlDownloadRequestTest, HashHeaders) {
  if (!UsingEmulator()) GTEST_SKIP();
  // Run one attempt and return the headers, if any.
  HashValues hashes;
  auto attempt = [&] {
    CurlRequestBuilder builder(HttpBinEndpoint() + "/response-headers",
                               GetDefaultCurlHandleFactory());
    builder.AddQueryParameter("x-goog-hash", "crc32c=123, md5=234");
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();
    auto constexpr kBufferSize = 4096;
    char buffer[kBufferSize];
    do {
      auto read = (*download)->Read(buffer, kBufferSize);
      if (!read) return read.status();
      hashes = Merge(std::move(hashes), std::move(read->hashes));
      if (read->response.status_code != 100) break;
    } while (true);
    return Status{};
  };

  auto status = Make3Attempts(attempt);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(hashes.crc32c, "123");
  EXPECT_EQ(hashes.md5, "234");
}

TEST(CurlDownloadRequestTest, Generation) {
  if (!UsingEmulator()) GTEST_SKIP();
  // Run one attempt and return the headers, if any.
  absl::optional<std::int64_t> received_generation;
  auto attempt = [&] {
    CurlRequestBuilder builder(HttpBinEndpoint() + "/response-headers",
                               GetDefaultCurlHandleFactory());
    builder.AddQueryParameter("x-goog-generation", "123456");
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();
    auto constexpr kBufferSize = 4096;
    char buffer[kBufferSize];
    do {
      auto read = (*download)->Read(buffer, kBufferSize);
      if (!read) return read.status();
      if (!received_generation && read->generation.has_value()) {
        received_generation = read->generation;
      }
      if (read->response.status_code != 100) break;
    } while (true);
    return Status{};
  };

  auto status = Make3Attempts(attempt);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(received_generation.value_or(0), 123456);
}

TEST(CurlDownloadRequestTest, HandlesReleasedOnRead) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto constexpr kLineCount = 10;
  auto constexpr kTestPoolSize = 8;
  auto factory = std::make_shared<rest_internal::PooledCurlHandleFactory>(
      kTestPoolSize, Options{});
  ASSERT_EQ(0, factory->CurrentHandleCount());
  ASSERT_EQ(0, factory->CurrentMultiHandleCount());

  auto download = [&]() -> Status {
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/stream/" + std::to_string(kLineCount), factory);
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();

    char buffer[4096];
    auto read = (*download)->Read(buffer, sizeof(buffer));
    if (!read) return std::move(read).status();
    // The data is 10 lines of about 200 bytes each, it all fits in the buffer.
    EXPECT_LT(read->bytes_received, sizeof(buffer));
    // This means the transfers completes during the Read() call, and the
    // handles are immediately returned to the pool.
    EXPECT_EQ(1, factory->CurrentHandleCount());
    EXPECT_EQ(1, factory->CurrentMultiHandleCount());

    auto close = (*download)->Close();
    if (!close) return std::move(close).status();
    EXPECT_EQ(1, factory->CurrentHandleCount());
    EXPECT_EQ(1, factory->CurrentMultiHandleCount());
    return Status{};
  };

  auto status = Make3Attempts(download);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(1, factory->CurrentHandleCount());
  EXPECT_EQ(1, factory->CurrentMultiHandleCount());
}

TEST(CurlDownloadRequestTest, HandlesReleasedOnClose) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto constexpr kLineCount = 10;
  auto constexpr kTestPoolSize = 8;
  auto factory = std::make_shared<rest_internal::PooledCurlHandleFactory>(
      kTestPoolSize, Options{});
  ASSERT_EQ(0, factory->CurrentHandleCount());
  ASSERT_EQ(0, factory->CurrentMultiHandleCount());

  auto download = [&]() -> Status {
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/stream/" + std::to_string(kLineCount), factory);
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();

    char buffer[4];
    auto read = (*download)->Read(buffer, sizeof(buffer));
    if (!read) return std::move(read).status();
    // The data is 10 lines of about 200 bytes each, it will not fit in the
    // buffer:
    EXPECT_EQ(read->bytes_received, sizeof(buffer));
    EXPECT_EQ(read->response.status_code, HttpStatusCode::kContinue);
    // This means the transfer is still active, and the handles would not have
    // been returned to the pool.
    EXPECT_EQ(0, factory->CurrentHandleCount());
    EXPECT_EQ(0, factory->CurrentMultiHandleCount());

    auto close = (*download)->Close();
    if (!close) return std::move(close).status();
    EXPECT_EQ(1, factory->CurrentHandleCount());
    EXPECT_EQ(1, factory->CurrentMultiHandleCount());
    return Status{};
  };

  auto status = Make3Attempts(download);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(1, factory->CurrentHandleCount());
  EXPECT_EQ(1, factory->CurrentMultiHandleCount());
  ASSERT_STATUS_OK(status);
}

TEST(CurlDownloadRequestTest, HandlesReleasedOnError) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto constexpr kTestPoolSize = 8;
  auto factory = std::make_shared<rest_internal::PooledCurlHandleFactory>(
      kTestPoolSize, Options{});
  ASSERT_EQ(0, factory->CurrentHandleCount());
  ASSERT_EQ(0, factory->CurrentMultiHandleCount());

  CurlRequestBuilder request("https://localhost:1/get", factory);
  auto download = std::move(request).BuildDownloadRequest();
  ASSERT_STATUS_OK(download);

  // This `.Read()` call fails as the endpoint is invalid.
  char buffer[4096];
  auto read = (*download)->Read(buffer, sizeof(buffer));
  ASSERT_THAT(read, Not(IsOk()));
  // Assuming there was an error the CURL* handle should not be returned to the
  // pool. The CURLM* handle is a local resource and always reusable so it does:
  EXPECT_EQ(0, factory->CurrentHandleCount());
  EXPECT_EQ(1, factory->CurrentMultiHandleCount());

  auto close = (*download)->Close();
  ASSERT_THAT(close, IsOk());
  EXPECT_THAT(0, close->status_code);
  // No changes expected in the pool sizes.
  EXPECT_EQ(0, factory->CurrentHandleCount());
  EXPECT_EQ(1, factory->CurrentMultiHandleCount());
}

TEST(CurlDownloadRequestTest, SimpleStreamReadAfterClosed) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto constexpr kLineCount = 10;
  auto download = [&]() -> StatusOr<std::string> {
    std::string contents;
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/stream/" + std::to_string(kLineCount),
        storage::internal::GetDefaultCurlHandleFactory());
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();
    // Perform a series of very small `.Read()` calls. libcurl provides data to
    // CurlDownloadRequest in chunks larger than 4 bytes. This forces
    // CurlDownloadRequest to keep data in its "spill" buffer, and to return the
    // data in the `Read()` requests even after the CURL* handle is closed.
    char buffer[4];
    do {
      auto result = (*download)->Read(buffer, sizeof(buffer));
      if (!result) return std::move(result).status();
      if (result->bytes_received == 0) break;
      contents += std::string{buffer, result->bytes_received};
    } while (true);
    return contents;
  };

  auto delay = std::chrono::seconds(1);
  StatusOr<std::string> received;
  for (int i = 0; i != 3; ++i) {
    received = download();
    if (received) break;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  ASSERT_STATUS_OK(received);
  std::vector<std::string> lines = absl::StrSplit(*received, '\n');
  auto p = std::remove(lines.begin(), lines.end(), std::string{});
  lines.erase(p, lines.end());
  EXPECT_EQ(kLineCount, lines.size());
  int count = 0;
  for (auto const& line : lines) {
    auto parsed = nlohmann::json::parse(line);
    ASSERT_TRUE(parsed.contains("id"));
    EXPECT_EQ(count++, parsed["id"].get<std::int64_t>());
  }
}

// Run one attempt of the Regression7051 test. This is wrapped in a retry loop,
// as integration tests flake due to unrelated (and unavoidable) problems, e.g.,
// trying to setup connections.
Status AttemptRegression7051() {
  // Download the maximum number of lines supported by httpbin.org
  auto constexpr kDownloadedLines = 100;
  auto constexpr kTestPoolSize = 32;
  auto factory = std::make_shared<rest_internal::PooledCurlHandleFactory>(
      kTestPoolSize, Options{});

  auto make_download = [&] {
    CurlRequestBuilder builder(
        HttpBinEndpoint() + "/stream/" + std::to_string(kDownloadedLines),
        factory);
    return std::move(builder).BuildDownloadRequest();
  };

  auto error = [](std::string msg) {
    return Status(StatusCode::kUnknown, std::move(msg));
  };

  auto constexpr kBufferSize = kDownloadedLines;
  char buffer[kBufferSize];

  void* id;
  {
    auto r_no_close = make_download();
    if (!r_no_close) return std::move(r_no_close).status();
    id = (*r_no_close)->id();
    if (id == nullptr) return error("r_no_close.id()==nulltptr");
    auto read = (*r_no_close)->Read(buffer, kBufferSize);
    if (!read) return std::move(read).status();
  }

  {
    auto r_partial_close = make_download();
    if (!r_partial_close) return std::move(r_partial_close).status();
    if ((*r_partial_close)->id() != id) {
      return error("r_partial_close.id() != id");
    }
    auto read = (*r_partial_close)->Read(buffer, kBufferSize);
    if (!read) return std::move(read).status();
    auto close = (*r_partial_close)->Close();
    if (!close) return std::move(close).status();
  }

  auto r_full = make_download();
  if (!r_full) return std::move(r_full).status();
  if ((*r_full)->id() != id) return error("r_full.id() != id");
  do {
    auto read = (*r_full)->Read(buffer, kBufferSize);
    if (!read) return std::move(read).status();
    if (read->response.status_code != 100) break;
  } while (true);
  auto close = (*r_full)->Close();
  if (!close) return std::move(close).status();

  return Status{};
}

/// @test Prevent regressions of #7051: re-using a stream after a partial read.
TEST(CurlDownloadRequestTest, Regression7051) {
  if (!UsingEmulator()) GTEST_SKIP();
  auto status = Make3Attempts(AttemptRegression7051);
  ASSERT_STATUS_OK(status);
}

#if CURL_AT_LEAST_VERSION(7, 43, 0)
TEST(CurlDownloadRequestTest, HttpVersion) {
  if (!UsingEmulator()) GTEST_SKIP();
  // Run one attempt and return the response.
  struct Response {
    std::multimap<std::string, std::string> headers;
    std::string payload;
  };
  auto attempt = [](std::string const& version) -> StatusOr<Response> {
    Response response;
    auto factory = std::make_shared<rest_internal::DefaultCurlHandleFactory>();
    CurlRequestBuilder builder(HttpBinEndpoint() + "/get", factory);
    builder.ApplyClientOptions(
        Options{}.set<storage_experimental::HttpVersionOption>(version));
    auto download = std::move(builder).BuildDownloadRequest();
    if (!download) return std::move(download).status();

    auto constexpr kBufferSize = 4096;
    char buffer[kBufferSize];
    do {
      auto read = (*download)->Read(buffer, kBufferSize);
      if (!read) return std::move(read).status();
      response.headers.insert(read->response.headers.begin(),
                              read->response.headers.end());
      response.payload += std::string{buffer, read->bytes_received};
      if (read->response.status_code != 100) break;
    } while (true);
    auto close = (*download)->Close();
    if (!close) return std::move(close).status();
    return response;
  };

  struct Test {
    std::string version;
    std::string prefix;
  } cases[] = {
      // The HTTP version setting is a request, libcurl may negotiate a
      // different version. For example, the server may not support HTTP/2.
      // Sadly this makes this test less interesting, but at least we check
      // that the request succeeds.
      {"1.0", "http/1"},
      {"1.1", "http/1"},
      {"2", "http/"},
      {"", "http/"},
  };

  auto* vinfo = curl_version_info(CURLVERSION_NOW);
  auto const supports_http2 = vinfo->features & CURL_VERSION_HTTP2;

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with version=<" + test.version + ">");
    auto delay = std::chrono::seconds(1);
    StatusOr<Response> response;
    for (int i = 0; i != 3; ++i) {
      response = attempt(test.version);
      if (response) break;
      std::this_thread::sleep_for(delay);
      delay *= 2;
    }
    EXPECT_STATUS_OK(response);
    if (!response) continue;
    EXPECT_THAT(response->headers, Contains(Pair(StartsWith(test.prefix), "")));

    // The httpbin.org site strips the `Connection` header.
    if (supports_http2 && test.version == "2") {
      auto parsed = nlohmann::json::parse(response->payload);
      auto const& request_headers = parsed["headers"];
      auto const& connection = request_headers.value("Connection", "");
      EXPECT_THAT(connection, HasSubstr("HTTP2")) << parsed;
    }
  }
}
#endif  // CURL_AT_LEAST_VERSION

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
