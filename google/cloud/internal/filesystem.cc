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

#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/throw_delegate.h"
#include <sys/types.h>
// The order of these two includes cannot be changed.
#include <sys/stat.h>
#if _WIN32
#else
#include <dirent.h>
#include <fcntl.h>
#endif  // _WIN32

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
using os_stat_type = struct ::_stat64;
inline int os_stat(std::string const& path, os_stat_type& s) {
  return ::_stat64(path.c_str(), &s);
}
#else
using os_stat_type = struct stat;
inline int os_stat(std::string const& path, os_stat_type& s) {
  return stat(path.c_str(), &s);
}
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
  os_stat_type stat{};
  ec.clear();
  int r = os_stat(path, stat);
#if _WIN32
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
  os_stat_type stat{};
  ec.clear();
  int r = os_stat(path, stat);
#if _WIN32
  if (r == -1) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  } else if (r == EINVAL) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  }
#else
  if (r != 0) {
    ec.assign(errno, std::generic_category());
    return static_cast<std::uintmax_t>(-1);
  }
#endif  // _WIN32
  return static_cast<std::uintmax_t>(stat.st_size);
}

std::string PathAppend(std::string const& directory, std::string const& path) {
#if _WIN32
  auto constexpr kSeparator = '\\';
  auto is_separator = [](char c) { return c == '\\' || c == '/'; };
#else
  auto constexpr kSeparator = '/';
  auto is_separator = [](char c) { return c == '/'; };
#endif
  if (path.empty()) return directory;
  if (directory.empty()) return path;
  if (!is_separator(directory.back()) && !is_separator(path.front())) {
    return directory + kSeparator + path;
  }
  if (!is_separator(directory.back()) || !is_separator(path.front())) {
    return directory + path;
  }
  auto r = directory;
  r.pop_back();
  r += path;
  return r;
}

std::vector<std::string> GetFileNames(std::string const& directory_path) {
  std::vector<std::string> filenames;

#ifndef _WIN32
  DIR* dir = opendir(directory_path.c_str());
  if (dir == nullptr) {
    return filenames;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string const filename = entry->d_name;
    if (filename == "." || filename == "..") {
      continue;
    }
    std::string file_fullname = directory_path + "/";
    file_fullname += filename;
    auto s = status(file_fullname);
    if (is_regular(s)) {
      filenames.push_back(filename);
    }
  }
  closedir(dir);
#endif

  return filenames;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
