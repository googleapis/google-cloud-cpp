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

#ifndef GOOGLECLOUDCPP_FIRESTORE_GOOGLE_FIRESTORE_FIELD_PATH_H_
#define GOOGLECLOUDCPP_FIRESTORE_GOOGLE_FIRESTORE_FIELD_PATH_H_

#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace firestore {
/**
 * A FieldPath refers to a field in a document. The path may consist of
 * a single field name (referring to a top level field in the document),
 * or a list of field names (referring to a nested field in the document).
 */
class FieldPath {
 public:
  // Construct FieldPath from a vector of field names
  FieldPath(std::vector<std::string> const parts);

  // Construct FieldPath from a field path string
  static const FieldPath FromString(std::string const& string);

  // The regex for a simple field name
  static std::regex simple_field_name;

  // Construct a new FieldPath by appending a field path string
  const FieldPath append(const std::string& path) const;

  // Construct a new FieldPath by appending a field path string
  const FieldPath append(const FieldPath& field_path) const;

  // Convert the FieldPath into a unique representation for the server
  const std::string ToApiRepr() const;

  // Return the length of components for this FieldPath
  size_t size() const { return parts.size(); }

  // Compare the equality of this FieldPath with another FieldPath
  bool operator==(const FieldPath& other) const;

  // Compare the non-equality of this FieldPath with another FieldPath
  bool operator!=(const FieldPath& other) const;

  // The representation of this FieldPath for ostream
  friend std::ostream& operator<<(std::ostream& os,
                                  const FieldPath& field_path);

 private:
  // Ensures string has no invalid characters
  static void check_invalid_characters(const std::string& string);

  // Splits string via field path delimiter '.'
  static const std::vector<std::string> split(std::string const string);

  // The components of this FieldPath
  std::vector<std::string> parts;
};
}  // namespace firestore
#endif
