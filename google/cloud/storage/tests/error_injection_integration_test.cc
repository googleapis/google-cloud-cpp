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
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <dlfcn.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_bucket_name;

class ErrorInjectionIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

class SymInterceptor {
 public:
  SymInterceptor();

  static SymInterceptor& Instance() {
    static SymInterceptor interceptor;
    return interceptor;
  }

  int LastSeenSendDescriptor() {
    if (!last_seen_send_fd_) {
      Terminate("send() has not been called yet.");
    }
    return *last_seen_send_fd_;
  }

  int LastSeenRecvDescriptor() {
    if (!last_seen_recv_fd_) {
      Terminate("recv() has not been called yet.");
    }
    return *last_seen_recv_fd_;
  }

  void StartFailingSend(int fd, int err, int num_failures = 0) {
    fail_send_ = FailDesc{fd, err, num_failures};
    num_failed_send_ = 0;
  }

  void StartFailingRecv(int fd, int err, int num_failures = 0) {
    fail_recv_ = FailDesc{fd, err, num_failures};
    num_failed_recv_ = 0;
  }

  std::size_t StopFailingSend() {
    fail_send_.reset();
    return num_failed_send_;
  }

  std::size_t StopFailingRecv() {
    fail_recv_.reset();
    return num_failed_recv_;
  }

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
  static thread_local optional<int> last_seen_send_fd_;
  static thread_local optional<FailDesc> fail_send_;
  static thread_local std::size_t num_failed_send_;
  static thread_local optional<int> last_seen_recv_fd_;
  static thread_local optional<FailDesc> fail_recv_;
  static thread_local std::size_t num_failed_recv_;
};

SymInterceptor::SymInterceptor()
    : orig_send_(GetOrigSymbol<SendPtr>("send")),
      orig_recv_(GetOrigSymbol<RecvPtr>("recv")) {}

thread_local optional<int> SymInterceptor::last_seen_send_fd_;
thread_local optional<SymInterceptor::FailDesc> SymInterceptor::fail_send_;
thread_local std::size_t SymInterceptor::num_failed_send_ = 0;
thread_local optional<int> SymInterceptor::last_seen_recv_fd_;
thread_local optional<SymInterceptor::FailDesc> SymInterceptor::fail_recv_;
thread_local std::size_t SymInterceptor::num_failed_recv_ = 0;

extern "C" ssize_t send(int sockfd, void const* buf, size_t len, int flags) {
  return SymInterceptor::Instance().Send(sockfd, buf, len, flags);
}

extern "C" ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return SymInterceptor::Instance().Recv(sockfd, buf, len, flags);
}

TEST_F(ErrorInjectionIntegrationTest, InjectRecvErrorOnRead) {
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
  is.exceptions(std::ios_base::badbit | std::ios_base::eofbit);
  std::vector<char> read_buf(opts->download_buffer_size() + 1);
  is.read(read_buf.data(), read_buf.size());
  SymInterceptor::Instance().StartFailingRecv(
      SymInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET);
  EXPECT_THROW(is.read(read_buf.data(), read_buf.size()),
               std::ios_base::failure);
  is.Close();
  EXPECT_EQ(StatusCode::kUnavailable, is.status().code());
  EXPECT_GE(SymInterceptor::Instance().StopFailingRecv(), 2);

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
  is.exceptions(std::ios_base::badbit | std::ios_base::eofbit);
  std::vector<char> read_buf(opts->download_buffer_size() + 1);
  is.read(read_buf.data(), read_buf.size());
  // The failed recv will trigger a retry, which includes sending.
  SymInterceptor::Instance().StartFailingRecv(
      SymInterceptor::Instance().LastSeenRecvDescriptor(), ECONNRESET, 1);
  SymInterceptor::Instance().StartFailingSend(
      SymInterceptor::Instance().LastSeenSendDescriptor(), ECONNRESET);
  EXPECT_THROW(is.read(read_buf.data(), read_buf.size()),
               std::ios_base::failure);
  is.Close();
  EXPECT_EQ(StatusCode::kUnavailable, is.status().code());
  EXPECT_GE(SymInterceptor::Instance().StopFailingSend(), 1);
  EXPECT_GE(SymInterceptor::Instance().StopFailingRecv(), 1);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

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
