// Copyright 2018 Google Inc.
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

#include "firestore/client/field_path.h"

std::regex simple_field_name("[_a-zA-Z][_a-zA-Z0-9]*");

namespace firestore {

FieldPath::FieldPath(std::vector<std::string> const parts) : parts_(parts) {
  for (const auto part : parts) {
    if (part.empty()) {
      throw std::invalid_argument("One or more components is empty.");
    }
  }
}

void FieldPath::CheckInvalidCharacters(std::string const& string) {
  std::array<char, 6> invalid_chars = {"~*/[]"};
  for (const auto invalid_char : invalid_chars) {
    if (string.find(invalid_char) != std::string::npos) {
      throw std::invalid_argument("Invalid characters in string");
    }
  }
}

const std::vector<std::string> FieldPath::split(std::string string) {
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

const FieldPath FieldPath::FromString(std::string const& string) {
  CheckInvalidCharacters(string);
  const auto parts = split(string);
  return FieldPath(parts);
};

const FieldPath FieldPath::append(std::string const& path) const {
  std::vector<std::string> parts(this->parts_);
  auto field_path = FieldPath::FromString(path);
  return this->append(field_path);
}

const FieldPath FieldPath::append(const FieldPath& field_path) const {
  std::vector<std::string> parts(this->parts);
  for (auto part : field_path.parts) {
    parts.push_back(part);
  }
  return FieldPath(parts);
}

const std::string FieldPath::ToApiRepr() const {
  std::string s;
  for (auto part : parts_) {
    auto match = std::regex_match(part, ::simple_field_name);
    if (std::regex_match(part, ::simple_field_name)) {
      s += part + '.';
    } else {
      part = std::regex_replace(part, std::regex("\\\\"), "\\\\");
      part = std::regex_replace(part, std::regex("`"), "\\`");
      s += '`' + part + "`.";
    }
  }
  s.resize(s.size() - 1); // cannot be empty and remove final period
  return s;
}

bool FieldPath::operator==(FieldPath const& other) const {
  return this->ToApiRepr() == other.ToApiRepr();
}

bool FieldPath::operator!=(const FieldPath& other) const {
  return !((*this) == other);
}

std::ostream& operator<<(std::ostream& os, FieldPath const& field_path) {
  os << field_path.ToApiRepr();
  return os;
}
}  // namespace firestore
