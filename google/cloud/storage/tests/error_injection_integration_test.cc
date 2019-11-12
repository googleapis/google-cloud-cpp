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

#include "google/cloud/optional.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/terminate_handler.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"
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

// Initialized in main() below.
char const* flag_bucket_name;

// This test uess dlsym(), which is not present on Windows.
// One could replace it with LoadLibrary() on Windows, but it's only a test, so
// it's not worth it.
#ifndef _WIN32

class ErrorInjectionIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

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
  optional<int> last_seen_send_fd_;
  optional<FailDesc> fail_send_;
  std::size_t num_failed_send_;
  optional<int> last_seen_recv_fd_;
  optional<FailDesc> fail_recv_;
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
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  // Make sure buffer is at least equal to curl max buffer size (which is 2MiB)
  opts->SetUploadBufferSize(2 * 1024 * 1024);

  Client client(*opts, LimitedTimeRetryPolicy(std::chrono::milliseconds(100)));

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  // Make sure the buffer is big enough to cause a flush.
  std::vector<char> buf(opts->upload_buffer_size() + 1, 'X');
  os << buf.data();
  SymbolInterceptor::Instance().StartFailingSend(
      SymbolInterceptor::Instance().LastSeenSendDescriptor(), ECONNRESET);
  os << buf.data();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.IsOpen());
  EXPECT_EQ(StatusCode::kUnavailable, os.last_status().code());

  SymbolInterceptor::Instance().StopFailingSend();
  os.Close();
  EXPECT_FALSE(os.metadata());
  EXPECT_FALSE(os.metadata().ok());
  EXPECT_EQ(StatusCode::kUnavailable, os.metadata().status().code());
  EXPECT_THAT(os.metadata().status().message(),
              ::testing::HasSubstr("Retry policy exhausted"));
}

TEST_F(ErrorInjectionIntegrationTest, InjectRecvErrorOnRead) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  // Make it at least the maximum curl buffer size (which is 512KiB)
  opts->SetDownloadBufferSize(512 * 1024);
  Client client(*opts, LimitedTimeRetryPolicy(std::chrono::milliseconds(500)),
                ExponentialBackoffPolicy(1_us, 2_us, 2));

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  WriteRandomLines(os, expected, 80, opts->download_buffer_size() * 3 / 80);
  os.Close();
  EXPECT_TRUE(os);
  EXPECT_TRUE(os.metadata().ok());

  auto is = client.ReadObject(bucket_name, object_name);
  std::vector<char> read_buf(opts->download_buffer_size() + 1);
  is.read(read_buf.data(), read_buf.size());
  SymbolInterceptor::Instance().StartFailingRecv(
      SymbolInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET, 10);
  is.read(read_buf.data(), read_buf.size());
  ASSERT_TRUE(is.status().ok());
  is.Close();
  EXPECT_GE(SymbolInterceptor::Instance().StopFailingRecv(), 2);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ErrorInjectionIntegrationTest, InjectSendErrorOnRead) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  // Make it at least the maximum curl buffer size (which is 512KiB)
  opts->SetDownloadBufferSize(512 * 1024);
  Client client(*opts, LimitedTimeRetryPolicy(std::chrono::milliseconds(500)));

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  os.exceptions(std::ios_base::badbit);

  WriteRandomLines(os, expected, 80, opts->download_buffer_size() * 3 / 80);
  os.Close();
  EXPECT_TRUE(os);
  EXPECT_TRUE(os.metadata().ok());

  auto is = client.ReadObject(bucket_name, object_name);
  std::vector<char> read_buf(opts->download_buffer_size() + 1);
  is.read(read_buf.data(), read_buf.size());
  // The failed recv will trigger a retry, which includes sending.
  SymbolInterceptor::Instance().StartFailingRecv(
      SymbolInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET, 1);
  SymbolInterceptor::Instance().StartFailingSend(
      SymbolInterceptor::Instance().LastSeenSendDescriptor(), ECONNRESET, 3);
  is.read(read_buf.data(), read_buf.size());
  ASSERT_FALSE(is.status().ok());
  is.Close();
  EXPECT_EQ(StatusCode::kUnavailable, is.status().code());
  EXPECT_GE(SymbolInterceptor::Instance().StopFailingSend(), 1);
  EXPECT_GE(SymbolInterceptor::Instance().StopFailingRecv(), 1);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

#endif  // _WIN32

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_bucket_name = argv[1];

  return RUN_ALL_TESTS();
}
