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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H_

#include "google/cloud/version.h"
#include <cinttypes>
#include <system_error>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

enum class file_type {
  none = 0,
  not_found,
  regular,
  directory,
  symlink,
  block,
  character,
  fifo,
  socket,
  unknown,
};

enum class perms {
  none = 0,
  owner_read = 0400,
  owner_write = 0200,
  owner_exec = 0100,
  owner_all = 0700,
  group_read = 0040,
  group_write = 0020,
  group_exec = 0010,
  group_all = 0070,
  others_read = 0004,
  others_write = 0002,
  others_exec = 0001,
  others_all = 0007,
  all = 0777,
  set_uid = 04000,
  set_gid = 02000,
  sticky_bit = 01000,
  mask = 07777,

  unknown = 0xFFFF,
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
class file_status {
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

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FILESYSTEM_H_
