// Copyright 2018 Google LLC
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

#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>
#include <fstream>
#if GTEST_OS_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#endif  // GTEST_OS_LINUX

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

std::string CreateRandomFileName() {
  static DefaultPRNG generator = MakeDefaultPRNG();
  // When running on the internal Google CI systems we cannot write to the local
  // directory, GTest has a good temporary directory in that case.
  return ::testing::TempDir() +
         Sample(generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
}

TEST(FilesystemTest, PermissionsOperatorBitand) {
  EXPECT_EQ(perms::none, perms::owner_all & perms::others_all);
  EXPECT_EQ(perms::owner_exec, perms::owner_all & perms::owner_exec);
}

TEST(FilesystemTest, PermissionsOperatorBitor) {
  EXPECT_EQ(0707U, static_cast<unsigned>(perms::owner_all | perms::others_all));
  EXPECT_EQ(perms::owner_all, perms::owner_all | perms::owner_exec);
}

TEST(FilesystemTest, PermissionsOperatorBitxor) {
  EXPECT_EQ(0707U, static_cast<unsigned>(perms::owner_all ^ perms::others_all));
  EXPECT_EQ(0600U, static_cast<unsigned>(perms::owner_all ^ perms::owner_exec));
}

TEST(FilesystemTest, PermissionsNegate) {
  EXPECT_EQ(07077U, static_cast<unsigned>(~perms::owner_all));
  EXPECT_EQ(07677U, static_cast<unsigned>(~perms::owner_exec));
  EXPECT_EQ(07707U, static_cast<unsigned>(~perms::group_all));
  EXPECT_EQ(07770U, static_cast<unsigned>(~perms::others_all));
}

TEST(FilesystemTest, PermissionsOperatorBitandEquals) {
  perms lhs = perms::owner_all;
  lhs &= perms::others_all;
  EXPECT_EQ(0U, static_cast<unsigned>(lhs));
}

TEST(FilesystemTest, PermissionsOperatorBitorEquals) {
  perms lhs = perms::owner_all;
  lhs |= perms::others_all;
  EXPECT_EQ(0707U, static_cast<unsigned>(lhs));
}

TEST(FilesystemTest, PermissionsOperatorBitxorEquals) {
  perms lhs = perms::owner_all;
  lhs ^= perms::owner_exec;
  EXPECT_EQ(0600U, static_cast<unsigned>(lhs));
}

TEST(FilesystemTest, StatusDirectory) {
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_directory(file_status));
}

TEST(FilesystemTest, StatusBlock) {
#if GTEST_OS_LINUX
  std::error_code ec;
  auto file_status = status("/dev/loop0", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  if (not exists(file_status)) {
    // In some CI builds there is no /dev/loop0, and no other well-known
    // block device comes to mind, simply stop the test when that happens.
    return;
  }
  EXPECT_TRUE(is_block_file(file_status));
  EXPECT_TRUE(is_other(file_status));
#else
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(is_block_file(file_status));
  EXPECT_FALSE(is_other(file_status));
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, StatusCharacter) {
#if GTEST_OS_LINUX
  std::error_code ec;
  auto file_status = status("/dev/null", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_character_file(file_status));
  EXPECT_TRUE(is_other(file_status));
#else
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(is_block_file(file_status));
  EXPECT_FALSE(is_other(file_status));
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, StatusFifo) {
#if GTEST_OS_LINUX
  auto file_name = CreateRandomFileName();
  mkfifo(file_name.c_str(), 0777);
  std::error_code ec;
  auto file_status = status(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_fifo(file_status));
  EXPECT_TRUE(is_other(file_status));
  std::remove(file_name.c_str());
#else
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(is_fifo(file_status));
  EXPECT_FALSE(is_other(file_status));
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, StatusRegular) {
  auto file_name = CreateRandomFileName();
  std::ofstream(file_name).close();
  std::error_code ec;
  auto file_status = status(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_regular(file_status));
  std::remove(file_name.c_str());
}

TEST(FilesystemTest, StatusSocket) {
#if GTEST_OS_LINUX
  auto file_name = CreateRandomFileName();
  int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  ASSERT_NE(-1, fd);

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, file_name.c_str(), sizeof(address.sun_path) - 1);
  int r = bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
  ASSERT_NE(-1, r);

  std::error_code ec;
  auto file_status = status(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_socket(file_status));
  EXPECT_TRUE(is_other(file_status));
  EXPECT_NE(-1, close(fd));
  std::remove(file_name.c_str());
#else
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(is_socket(file_status));
  EXPECT_FALSE(is_other(file_status));
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, StatusSymlink) {
  // status() is supposed to follow symbolic links, there is a different
  // function in C++17 (which we are not implementing) that does not follow
  // symbolic links.
#if GTEST_OS_LINUX
  auto file_name = CreateRandomFileName();
  std::ofstream(file_name).close();

  auto symbolic_link = CreateRandomFileName();
  ASSERT_NE(-1, link(file_name.c_str(), symbolic_link.c_str()));

  std::error_code ec;
  auto file_status = status(symbolic_link, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_TRUE(is_regular(file_status));
  EXPECT_FALSE(is_symlink(file_status));

  std::remove(symbolic_link.c_str());
  std::remove(file_name.c_str());
#else
  std::error_code ec;
  auto file_status = status(".", ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(is_fifo(file_status));
  EXPECT_FALSE(is_other(file_status));
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, StatusNotFound) {
  auto file_name = CreateRandomFileName();
  std::error_code ec;
  auto file_status = status(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_FALSE(exists(file_status));
  EXPECT_EQ(file_type::not_found, file_status.type());
}

TEST(FilesystemTest, StatusNotFoundDoesNotThrow) {
  auto file_name = CreateRandomFileName();
  file_status fs;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(fs = status(file_name));
#else
  fs = status(file_name);
  SUCCEED();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_FALSE(exists(fs));
  EXPECT_EQ(file_type::not_found, fs.type());
}

TEST(FilesystemTest, StatusAccessDoesNotThrow) {
#if GTEST_OS_LINUX
  auto file_name = "/proc/1/fd/0";
  file_status fs;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(fs = status(file_name));
#else
  fs = status(file_name);
  SUCCEED();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Do not check the returned file type, in some CI builds the tests run
  // as root, and therefore have full access to all the files and directories.
  // EXPECT_EQ(file_type::unknown, fs.type());
#endif
}

TEST(FilesystemTest, StatusErrorDoesThrow) {
#if GTEST_OS_LINUX
  auto file_name = CreateRandomFileName();
  std::ofstream(file_name).close();
  auto path = file_name + "/files/cannot/be/directories";
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { status(path); } catch (std::system_error const& ex) {
        EXPECT_EQ(static_cast<int>(std::errc::not_a_directory),
                  ex.code().value());
        EXPECT_THAT(ex.what(), HasSubstr(path));
        throw;
      },
      std::system_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(status(path), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  std::remove(file_name.c_str());
#else
  // I (coryan) do not know of a way to generate an error on Windows. One would
  // need to pass a null string (not empty, null) or something similar, and the
  // function signatures do not allow for that type of mistake.
#endif  // GTEST_OS_LINUX
}

TEST(FilesystemTest, FileSize) {
  auto file_name = CreateRandomFileName();
  std::ofstream os(file_name);
  os << std::string(1000, ' ');
  os.close();
  std::error_code ec;
  auto size = file_size(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_EQ(1000U, size);
  std::remove(file_name.c_str());
}

TEST(FilesystemTest, FileSizeEmpty) {
  auto file_name = CreateRandomFileName();
  std::ofstream(file_name).close();
  std::error_code ec;
  auto size = file_size(file_name, ec);
  EXPECT_FALSE(static_cast<bool>(ec));
  EXPECT_EQ(0U, size);
  std::remove(file_name.c_str());
}

TEST(FilesystemTest, FileSizeNotFound) {
  auto file_name = CreateRandomFileName();
  std::error_code ec;
  auto size = file_size(file_name, ec);
  EXPECT_TRUE(static_cast<bool>(ec));
  EXPECT_EQ(std::uintmax_t(-1), size);
}

TEST(FilesystemTest, FileSizeNotFoundDoesThrow) {
  auto path = CreateRandomFileName();
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { file_size(path); } catch (std::system_error const& ex) {
        EXPECT_EQ(static_cast<int>(std::errc::no_such_file_or_directory),
                  ex.code().value());
        EXPECT_THAT(ex.what(), HasSubstr(path));
        throw;
      },
      std::system_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(file_size(path), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
