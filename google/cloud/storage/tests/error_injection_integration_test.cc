// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/terminate_handler.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _us;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

// This test uses dlsym(), which is not present on Windows.
// One could replace it with LoadLibrary() on Windows, but it's only a test, so
// it's not worth it.
#ifndef _WIN32

class ErrorInjectionIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    if (!UsingEmulator()) GTEST_SKIP();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

/**
 * Object of this class is an interface to intercept symbols from libc.
 *
 * It's a singleton, and it's only instance should be where the control flow
 * from the original symbols should be directed.
 *
 * The intercepted symbols can be configured to return failures.
 */
class SymbolInterceptor {
 public:
  static SymbolInterceptor& Instance() {
    static SymbolInterceptor interceptor;
    return interceptor;
  }

  // FD most recently passed to send().
  int LastSeenSendDescriptor() {
    if (!last_seen_send_fd_) {
      Terminate("send() has not been called yet.");
    }
    return *last_seen_send_fd_;
  }

  // FD most recently passed to recv().
  int LastSeenRecvDescriptor() {
    if (!last_seen_recv_fd_) {
      Terminate("recv() has not been called yet.");
    }
    return *last_seen_recv_fd_;
  }

  /**
   * Start failing send().
   *
   * @param fd only the calls passing this FD will fail
   * @param err the error code to fail with
   * @param num_failures fail this many times and then get back to normal
   */
  void StartFailingSend(int fd, int err, int num_failures = 0) {
    fail_send_ = FailDesc{fd, err, num_failures};
    num_failed_send_ = 0;
  }

  /**
   * Start failing recv().
   *
   * @param fd only the calls passing this FD will fail
   * @param err the error code to fail with
   * @param num_failures fail this many times and then get back to normal
   */
  void StartFailingRecv(int fd, int err, int num_failures = 0) {
    fail_recv_ = FailDesc{fd, err, num_failures};
    num_failed_recv_ = 0;
  }

  /**
   * Stop failing send().
   *
   * @return how many times send() failed since last `StartFailingSend()` call.
   */
  std::size_t StopFailingSend() {
    fail_send_.reset();
    return num_failed_send_;
  }

  /**
   * Stop failing recv().
   *
   * @return how many times recv() failed since last `StartFailingRecv()` call.
   */
  std::size_t StopFailingRecv() {
    fail_recv_.reset();
    return num_failed_recv_;
  }

  // Entry point for the intercepted send()
  ssize_t Send(int sockfd, void const* buf, size_t len, int flags) {
    last_seen_send_fd_ = sockfd;
    if (fail_send_ && fail_send_->fd == sockfd) {
      ++num_failed_send_;
      errno = fail_send_->err;
      if (fail_send_->num_left != 0 && --fail_send_->num_left == 0) {
        StopFailingSend();
      }
      return -1;
    }
    return orig_send_(sockfd, buf, len, flags);
  }

  // Entry point for the intercepted recv()
  ssize_t Recv(int sockfd, void* buf, size_t len, int flags) {
    last_seen_recv_fd_ = sockfd;
    if (fail_recv_ && fail_recv_->fd == sockfd) {
      ++num_failed_recv_;
      errno = fail_recv_->err;
      if (fail_recv_->num_left != 0 && --fail_recv_->num_left == 0) {
        StopFailingRecv();
      }
      return -1;
    }
    return orig_recv_(sockfd, buf, len, flags);
  }

 private:
  SymbolInterceptor();

  using SendPtr = ssize_t (*)(int, void const*, size_t, int);
  using RecvPtr = ssize_t (*)(int, void*, size_t, int);

  struct FailDesc {
    int fd;
    int err;
    int num_left;
  };

  template <typename SymbolType>
  static SymbolType GetOrigSymbol(char const* symbol_name) {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    void* res = dlsym(RTLD_NEXT, symbol_name);
    if (res == nullptr) {
      std::stringstream stream;
      stream << "Can't capture the original " << symbol_name
             << "(): " << dlerror();
      Terminate(stream.str().c_str());
    }
    return reinterpret_cast<SymbolType>(res);
  }

  SendPtr orig_send_;
  RecvPtr orig_recv_;
  absl::optional<int> last_seen_send_fd_;
  absl::optional<FailDesc> fail_send_;
  std::size_t num_failed_send_;
  absl::optional<int> last_seen_recv_fd_;
  absl::optional<FailDesc> fail_recv_;
  std::size_t num_failed_recv_;
};

SymbolInterceptor::SymbolInterceptor()
    : orig_send_(GetOrigSymbol<SendPtr>("send")),
      orig_recv_(GetOrigSymbol<RecvPtr>("recv")) {}

extern "C" ssize_t send(int sockfd, void const* buf, size_t len, int flags) {
  return SymbolInterceptor::Instance().Send(sockfd, buf, len, flags);
}

extern "C" ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return SymbolInterceptor::Instance().Recv(sockfd, buf, len, flags);
}

TEST_F(ErrorInjectionIntegrationTest, InjectErrorOnStreamingWrite) {
  // Make sure buffer is at least equal to curl max buffer size (which is 2MiB)
  auto constexpr kUploadBufferSize = 2 * 1024 * 1024;
  auto client = Client(
      Options{}
          .set<UploadBufferSizeOption>(kUploadBufferSize)
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(std::chrono::milliseconds(100)).clone()));

  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  // Make sure the buffer is big enough to cause a flush.
  std::vector<char> buf(kUploadBufferSize + 1, 'X');
  os << buf.data();
  SymbolInterceptor::Instance().StartFailingSend(
      SymbolInterceptor::Instance().LastSeenSendDescriptor(), ECONNRESET);
  os << buf.data();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.IsOpen());
  EXPECT_THAT(os.last_status(), StatusIs(StatusCode::kUnavailable));

  SymbolInterceptor::Instance().StopFailingSend();
  os.Close();
  EXPECT_FALSE(os.metadata());
  EXPECT_THAT(os.metadata(), StatusIs(StatusCode::kUnavailable,
                                      HasSubstr("Retry policy exhausted")));
}

TEST_F(ErrorInjectionIntegrationTest, InjectRecvErrorOnRead) {
  auto constexpr kInjectedErrors = 10;
  // Make it at least the maximum curl buffer size (which is 512KiB)
  auto constexpr kDownloadBufferSize = 512 * 1024;
  auto client =
      Client(Options{}
                 .set<DownloadBufferSizeOption>(kDownloadBufferSize)
                 .set<RetryPolicyOption>(
                     LimitedErrorCountRetryPolicy(kInjectedErrors).clone())
                 .set<BackoffPolicyOption>(
                     ExponentialBackoffPolicy(1_us, 2_us, 2).clone()));

  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  WriteRandomLines(os, expected, 80,
                   static_cast<int>(kDownloadBufferSize) * 3 / 80);
  os.Close();
  EXPECT_TRUE(os);
  ASSERT_STATUS_OK(os.metadata());
  ScheduleForDelete(*os.metadata());

  auto is = client.ReadObject(bucket_name_, object_name);
  std::vector<char> read_buf(kDownloadBufferSize + 1);
  is.read(read_buf.data(), read_buf.size());
  SymbolInterceptor::Instance().StartFailingRecv(
      SymbolInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET,
      kInjectedErrors);
  is.read(read_buf.data(), read_buf.size());
  ASSERT_STATUS_OK(is.status());
  is.Close();
  EXPECT_EQ(SymbolInterceptor::Instance().StopFailingRecv(), kInjectedErrors);
}

TEST_F(ErrorInjectionIntegrationTest, InjectSendErrorOnRead) {
  // Make it at least the maximum curl buffer size (which is 512KiB)
  auto constexpr kDownloadBufferSize = 512 * 1024;
  auto client = Client(
      Options{}
          .set<DownloadBufferSizeOption>(kDownloadBufferSize)
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(std::chrono::milliseconds(500)).clone()));

  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  os.exceptions(std::ios_base::badbit);

  WriteRandomLines(os, expected, 80, kDownloadBufferSize * 3 / 80);
  os.Close();
  EXPECT_TRUE(os);
  ASSERT_STATUS_OK(os.metadata());
  ScheduleForDelete(*os.metadata());

  auto is = client.ReadObject(bucket_name_, object_name);
  std::vector<char> read_buf(kDownloadBufferSize + 1);
  is.read(read_buf.data(), read_buf.size());
  // The failed recv will trigger a retry, which includes sending.
  SymbolInterceptor::Instance().StartFailingRecv(
      SymbolInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET, 1);
  SymbolInterceptor::Instance().StartFailingSend(
      SymbolInterceptor::Instance().LastSeenSendDescriptor(), ECONNRESET, 3);
  is.read(read_buf.data(), read_buf.size());
  EXPECT_THAT(is.status(), Not(IsOk()));
  is.Close();
  EXPECT_THAT(is.status(), StatusIs(StatusCode::kUnavailable));
  EXPECT_GE(SymbolInterceptor::Instance().StopFailingSend(), 1);
  EXPECT_GE(SymbolInterceptor::Instance().StopFailingRecv(), 1);
}

#endif  // _WIN32

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
