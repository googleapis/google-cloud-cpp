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
#include "google/cloud/internal/throw_delegate.h"
#include <sys/types.h>
// The order of these two includes cannot be changed.
#include <sys/stat.h>
#if _WIN32
#else
#include <fcntl.h>
#endif  // _WIN32

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// NOLINTNEXTLINE(readability-identifier-naming)
file_status status(std::string const& path) {
  std::error_code ec;
  auto s = status(path, ec);
  if (ec) {
    std::string msg = __func__;
    msg += ": getting status of file=";
    msg += path;
    ThrowSystemError(ec, msg);
  }
  return s;
}

#if _WIN32
using os_stat_type = struct ::_stat;
#else
using os_stat_type = struct stat;
#endif  // _WIN32

perms ExtractPermissions(os_stat_type const& s) {
#if _WIN32
  // On Windows only a few permissions are available.
  perms permissions = perms::none;
  if (s.st_mode & _S_IREAD) {
    permissions |= perms::owner_read;
  }
  if (s.st_mode & _S_IWRITE) {
    permissions |= perms::owner_write;
  }
  if (s.st_mode & _S_IEXEC) {
    permissions |= perms::owner_exec;
  }
  return permissions;
#else
  // This depends on the fact that the permission bits in POSIX systems match
  // the definition of the `permissions` enum class.
  return static_cast<perms>(s.st_mode & static_cast<unsigned>(perms::mask));
#endif  // _WIN32
}

file_type ExtractFileType(os_stat_type const& s) {
#if _WIN32
  if (s.st_mode & _S_IFREG) {
    return file_type::regular;
  }
  if (s.st_mode & _S_IFDIR) {
    return file_type::directory;
  }
  if (s.st_mode & _S_IFCHR) {
    return file_type::character;
  }
#else
  if (S_ISREG(s.st_mode)) {
    return file_type::regular;
  }
  if (S_ISDIR(s.st_mode)) {
    return file_type::directory;
  }
  if (S_ISBLK(s.st_mode)) {
    return file_type::block;
  }
  if (S_ISCHR(s.st_mode)) {
    return file_type::character;
  }
  if (S_ISFIFO(s.st_mode)) {
    return file_type::fifo;
  }
  if (S_ISSOCK(s.st_mode)) {
    return file_type::socket;
  }
#endif  // _WIN32
  return file_type::unknown;
}

// NOLINTNEXTLINE(readability-identifier-naming)
file_status status(std::string const& path, std::error_code& ec) noexcept {
  os_stat_type stat;
  ec.clear();
#if _WIN32
  int r = ::_stat(path.c_str(), &stat);
  if (r == -1) {
    if (errno == ENOENT) {
      return file_status(file_type::not_found);
    }
    ec.assign(errno, std::generic_category());
    return {};
  } else if (r == EINVAL) {
    ec.assign(errno, std::generic_category());
    return {};
  }
#else
  int r = ::stat(path.c_str(), &stat);
  if (r != 0) {
    if (errno == EACCES) {
      return file_status(file_type::unknown);
    }
    if (errno == ENOENT) {
      return file_status(file_type::not_found);
    }
    ec.assign(errno, std::generic_category());
    return {};
  }
#endif  // _WIN32
  return file_status(ExtractFileType(stat), ExtractPermissions(stat));
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::uintmax_t file_size(std::string const& path) {
  std::error_code ec;
  auto s = file_size(path, ec);
  if (ec) {
    std::string msg = __func__;
    msg += ": getting size of file=";
    msg += path;
    ThrowSystemError(ec, msg);
  }
  return s;
}

// NOLINTNEXTLINE(readability-identifier-naming)
std::uintmax_t file_size(std::string const& path,
                         std::error_code& ec) noexcept {
  os_stat_type stat;
  ec.clear();
#if _WIN32
  int r = ::_stat(path.c_str(), &stat);
  if (r == -1) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  } else if (r == EINVAL) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  }
#else
  int r = ::stat(path.c_str(), &stat);
  if (r != 0) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  }
#endif  // _WIN32
  return static_cast<std::uintmax_t>(stat.st_size);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
