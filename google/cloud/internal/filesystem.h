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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H

#include "google/cloud/version.h"
#include <cinttypes>
#include <string>
#include <system_error>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

enum class file_type {
  none = 0,   // NOLINT(readability-identifier-naming)
  not_found,  // NOLINT(readability-identifier-naming)
  regular,    // NOLINT(readability-identifier-naming)
  directory,  // NOLINT(readability-identifier-naming)
  symlink,    // NOLINT(readability-identifier-naming)
  block,      // NOLINT(readability-identifier-naming)
  character,  // NOLINT(readability-identifier-naming)
  fifo,       // NOLINT(readability-identifier-naming)
  socket,     // NOLINT(readability-identifier-naming)
  unknown,    // NOLINT(readability-identifier-naming)
};

enum class perms {
  none = 0,             // NOLINT(readability-identifier-naming)
  owner_read = 0400,    // NOLINT(readability-identifier-naming)
  owner_write = 0200,   // NOLINT(readability-identifier-naming)
  owner_exec = 0100,    // NOLINT(readability-identifier-naming)
  owner_all = 0700,     // NOLINT(readability-identifier-naming)
  group_read = 0040,    // NOLINT(readability-identifier-naming)
  group_write = 0020,   // NOLINT(readability-identifier-naming)
  group_exec = 0010,    // NOLINT(readability-identifier-naming)
  group_all = 0070,     // NOLINT(readability-identifier-naming)
  others_read = 0004,   // NOLINT(readability-identifier-naming)
  others_write = 0002,  // NOLINT(readability-identifier-naming)
  others_exec = 0001,   // NOLINT(readability-identifier-naming)
  others_all = 0007,    // NOLINT(readability-identifier-naming)
  all = 0777,           // NOLINT(readability-identifier-naming)
  set_uid = 04000,      // NOLINT(readability-identifier-naming)
  set_gid = 02000,      // NOLINT(readability-identifier-naming)
  sticky_bit = 01000,   // NOLINT(readability-identifier-naming)
  mask = 07777,         // NOLINT(readability-identifier-naming)

  unknown = 0xFFFF,  // NOLINT(readability-identifier-naming)
};

inline perms operator&(perms lhs, perms rhs) {
  return static_cast<perms>(static_cast<unsigned>(lhs) &
                            static_cast<unsigned>(rhs));
}

inline perms operator|(perms lhs, perms rhs) {
  return static_cast<perms>(static_cast<unsigned>(lhs) |
                            static_cast<unsigned>(rhs));
}

inline perms operator^(perms lhs, perms rhs) {
  return static_cast<perms>(static_cast<unsigned>(lhs) ^
                            static_cast<unsigned>(rhs));
}

inline perms operator~(perms lhs) {
  return static_cast<perms>(static_cast<unsigned>(perms::mask) &
                            ~static_cast<unsigned>(lhs));
}

inline perms& operator&=(perms& lhs, perms rhs) {
  lhs = lhs & rhs;
  return lhs;
}

inline perms operator|=(perms& lhs, perms rhs) {
  lhs = lhs | rhs;
  return lhs;
}

inline perms operator^=(perms& lhs, perms rhs) {
  lhs = lhs ^ rhs;
  return lhs;
}

/**
 * A drop-in replacement for `std::filesystem::file_status`.
 *
 * Implement the C++17 `std::filesystem::file_status` class.
 */
class file_status {  // NOLINT(readability-identifier-naming)
 public:
  file_status() noexcept : file_status(file_type::none) {}
  explicit file_status(file_type type, perms permissions = perms::unknown)
      : type_(type), permissions_(permissions) {}
  file_status(file_status const&) noexcept = default;
  file_status(file_status&&) noexcept = default;
  file_status& operator=(file_status const&) noexcept = default;
  file_status& operator=(file_status&&) noexcept = default;

  file_type type() const noexcept { return type_; }
  void type(file_type type) noexcept { type_ = type; }
  perms permissions() const noexcept { return permissions_; }
  void permissions(perms permissions) noexcept { permissions_ = permissions; }

 private:
  file_type type_;
  perms permissions_;
};

file_status status(std::string const& path);
file_status status(std::string const& path, std::error_code& ec) noexcept;

inline bool status_known(file_status s) noexcept {
  return s.type() != file_type::none;
}

inline bool is_block_file(file_status s) noexcept {
  return s.type() == file_type::block;
}

inline bool is_character_file(file_status s) noexcept {
  return s.type() == file_type::character;
}

inline bool is_directory(file_status s) noexcept {
  return s.type() == file_type::directory;
}

inline bool is_fifo(file_status s) noexcept {
  return s.type() == file_type::fifo;
}

inline bool is_regular(file_status s) noexcept {
  return s.type() == file_type::regular;
}

inline bool is_socket(file_status s) noexcept {
  return s.type() == file_type::socket;
}

inline bool is_symlink(file_status s) noexcept {
  return s.type() == file_type::symlink;
}

inline bool exists(file_status s) noexcept {
  return status_known(s) && s.type() != file_type::not_found;
}

inline bool is_other(file_status s) noexcept {
  return exists(s) && !is_regular(s) && !is_directory(s) && !is_symlink(s);
}

std::uintmax_t file_size(std::string const& path);
std::uintmax_t file_size(std::string const& path, std::error_code& ec) noexcept;

/// Append @p path (even if it is an absolute path) to @p directory
std::string PathAppend(std::string const& directory, std::string const& path);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H
