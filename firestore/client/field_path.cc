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

#include "field_path.h"

namespace firestore {
std::regex FieldPath::simple_field_name("[_a-zA-Z][_a-zA-Z0-9]*");

FieldPath::FieldPath(const std::vector<std::string>& parts) : parts(parts) {
  for (const auto part : parts) {
    if (part.empty()) {
      throw std::invalid_argument("One or more components is empty.");
    }
  }
}

void FieldPath::check_invalid_characters(const std::string& string) {
  std::array<char, 6> invalid_chars = {"~*/[]"};
  for (const auto invalid_char : invalid_chars) {
    if (string.find(invalid_char) != std::string::npos) {
      throw std::invalid_argument("Invalid characters in string");
    }
  }
}

std::vector<std::string> FieldPath::split(const std::string& string) {
  std::vector<std::string> parts;
  auto copy(string);
  auto index = copy.find('.');
  while (index != std::string::npos) {
    parts.push_back(copy.substr(0, index));
    copy = copy.substr(index + 1);
    index = copy.find('.');
  }
  parts.push_back(copy);
  return parts;
}

const FieldPath FieldPath::from_string(const std::string& string) {
  check_invalid_characters(string);
  const auto parts = split(string);
  return FieldPath(parts);
};

const FieldPath FieldPath::append(const std::string& path) const {
  std::vector<std::string> parts(this->parts);
  auto field_path = FieldPath::from_string(path);
  return this->append(field_path);
}

const FieldPath FieldPath::append(const FieldPath& field_path) const {
  std::vector<std::string> parts(this->parts);
  for (auto part : field_path.parts) {
    parts.push_back(part);
  }
  return FieldPath(parts);
}

const std::string FieldPath::to_api_repr() const {
  std::string s;
  for (auto part : parts) {
    auto match = std::regex_match(part, simple_field_name);
    if (std::regex_match(part, simple_field_name)) {
      s += part + '.';
    } else {
      part = std::regex_replace(part, std::regex("\\\\"), "\\\\");
      part = std::regex_replace(part, std::regex("`"), "\\`");
      s += '`' + part + "`.";
    }
  }
  return s.substr(0, s.size() - 1);
}

bool FieldPath::operator==(const FieldPath& other) const {
  return this->to_api_repr() == other.to_api_repr();
}

bool FieldPath::operator!=(const FieldPath& other) const {
  return !((*this) == other);
}

std::ostream& operator<<(std::ostream& os, const FieldPath& field_path) {
  os << field_path.to_api_repr();
  return os;
}
}  // namespace firestore
