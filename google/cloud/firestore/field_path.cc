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

#include "google/cloud/firestore/field_path.h"
#include <array>
#include <cctype>

namespace google {
namespace cloud {
namespace firestore {

FieldPath::FieldPath(std::vector<std::string> parts)
    : parts_(std::move(parts)) {
  for (auto const& part : parts_) {
    if (part.empty()) {
      this->valid_ = false;
      return;
    }
  }
  this->valid_ = true;
}

FieldPath FieldPath::InvalidFieldPath() {
  std::vector<std::string> parts = {""};
  return FieldPath(parts);
}

FieldPath FieldPath::FromString(std::string const& string) {
  if (InvalidCharacters(string)) {
    return FieldPath::InvalidFieldPath();
  }
  return FieldPath(Split(string));
}

FieldPath FieldPath::Append(std::string const& string) const {
  std::vector<std::string> parts(this->parts_);
  auto const field_path = FieldPath::FromString(string);
  return this->Append(field_path);
}

FieldPath FieldPath::Append(FieldPath const& field_path) const {
  if (valid_ && field_path.valid_) {
    std::vector<std::string> parts(this->parts_);
    for (auto const& part : field_path.parts_) {
      parts.push_back(part);
    }
    return FieldPath(parts);
  }
  return FieldPath::InvalidFieldPath();
}

std::string FieldPath::ToApiRepr() const {
  // gcc-4.8 ships with a broken regex library (sigh), so don't use it.
  auto is_simple_field_name = [](std::string const& part) {
    if (part.empty()) {
      return false;
    }
    if (part[0] != '_' && std::isalpha(part[0]) == 0) {
      return false;
    }
    return std::all_of(part.begin(), part.end(),
                       [](char c) { return c == '_' || std::isalnum(c) != 0; });
  };
  std::string s;
  if (valid_) {
    for (auto part : parts_) {
      auto const match = is_simple_field_name(part);
      if (match) {
        s += part + '.';
      } else {
        ReplaceAll(part, "\\", "\\\\");
        ReplaceAll(part, "`", "\\`");
        s += '`' + part + "`.";
      }
    }
    s.resize(s.size() - 1);  // cannot be empty and remove final period
  }
  return s;  // let the server catch the empty string error for invalid
}

bool operator==(FieldPath const& lhs, FieldPath const& rhs) {
  return lhs.ToApiRepr() == rhs.ToApiRepr();
}

bool operator<(FieldPath const& lhs, FieldPath const& rhs) {
  auto const lhs_size = lhs.parts_.size();
  auto const rhs_size = rhs.parts_.size();
  auto const min_length = (std::min)(lhs_size, rhs_size);
  for (auto i = 0U; i != min_length; i++) {
    if (lhs.parts_[i] < rhs.parts_[i]) {
      return true;
    }
    if (lhs.parts_[i] > rhs.parts_[i]) {
      return false;
    }
  }
  if (lhs_size < rhs_size) {
    return true;
  }
  if (lhs_size > rhs_size) {
    return false;
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, FieldPath const& field_path) {
  os << field_path.ToApiRepr();
  return os;
}

bool FieldPath::InvalidCharacters(std::string const& string) {
  std::array<char, 6> const invalid_chars = {"~*/[]"};
  for (auto const invalid_char : invalid_chars) {
    if (string.find(invalid_char) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> FieldPath::Split(std::string string) {
  std::vector<std::string> parts;
  auto index = string.find('.');
  while (index != std::string::npos) {
    parts.push_back(string.substr(0, index));
    string = string.substr(index + 1);
    index = string.find('.');
  }
  parts.push_back(string);
  return parts;
}

void FieldPath::ReplaceAll(std::string& string, std::string const& find,
                           std::string const& replace) {
  auto found = string.find(find);
  while (found != std::string::npos) {
    string = string.replace(found, find.size(), replace);
    found += replace.size();
    found = string.find(find, found);
  }
}

}  // namespace firestore
}  // namespace cloud
}  // namespace google
