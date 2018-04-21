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

#ifndef GOOGLE_CLOUD_CPP_FIRESTORE_GOOGLE_FIRESTORE_FIELD_PATH_H_
#define GOOGLE_CLOUD_CPP_FIRESTORE_GOOGLE_FIRESTORE_FIELD_PATH_H_

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
  /**
   * Construct FieldPath from a vector of field name *parts*.
   * @param parts.  A const vector of strings which are field path components.
   */
  FieldPath(std::vector<std::string> const parts);

  /**
   * Construct an invalid FieldPath.
   */
  static FieldPath InvalidFieldPath();

  /**
   * Construct FieldPath from a field path string *string*.
   * @param string.  A const field path string for creating a FieldPath.
   */
  static FieldPath FromString(std::string const& string);

  /**
   * Construct a new FieldPath by appending a field path string *string*.
   * @param string.  A const field path string to append to this FieldPath.
   */

  FieldPath Append(std::string const& string) const;

  /**
   * Construct a new FieldPath by appending a FieldPath *field_path*.
   * @param field_path.  A const FieldPath to append to this FieldPath.
   */
  FieldPath Append(FieldPath const& field_path) const;

  /**
   * Convert the FieldPath into a unique representation for the server.
   */
  std::string ToApiRepr() const;

  /**
   * Return the length of components for this FieldPath.
   */
  size_t size() const { return parts_.size(); }

  /**
   * Compare the equality of this FieldPath with another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator==(FieldPath const& other) const;

  /**
   * Compare the non-equality of this FieldPath with another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator!=(FieldPath const& other) const;

  /**
   * Compare if this FieldPath is before another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator<(FieldPath const& other) const;

  /**
   * Compare if this FieldPath is before or equal to another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator<=(FieldPath const& other) const;

  /**
   * Compare if this FieldPath is after another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator>(FieldPath const& other) const;

  /**
   * Compare if this FieldPath is after or equal to another FieldPath *other*.
   * @param other.  The other const FieldPath to compare to.
   */
  bool operator>=(FieldPath const& other) const;

  /**
   * The representation of this FieldPath *field_path* for ostream *os*.
   * @param os.  ostream to write to.
   * @param field_path.  This const FieldPath.
   */
  friend std::ostream& operator<<(std::ostream& os,
                                  const FieldPath& field_path);

  /**
   * Returns whether this FieldPath is valid or not.
   */
  bool valid() const { return valid_; }

 private:
  /**
   * Ensures *string* has no invalid characters.
   * @param string.  A const string to write to.
   */
  static bool InvalidCharacters(std::string const& string);

  /**
   * Splits *string* via field path delimiter '.'.
   * @param string.  A const string to write to.
   */
  static std::vector<std::string> Split(std::string const string);

  /**
   * Replace all occurences of *find* in *string* with *replace*.
   * @param string.  A string to search and replace
   * @param find.  A const String to find and replace with *replace*
   * @param replace.  A const String to replace *find* with.
   */
  static void ReplaceAll(std::string& string, std::string const& find,
                         std::string const& replace);

  /**
   * The components of this FieldPath.
   */
  std::vector<std::string> parts_;

  /**
   * Whether this FieldPath is valid or not.
   */
  bool valid_;
};
}  // namespace firestore

#endif  // GOOGLE_CLOUD_CPP_FIRESTORE_GOOGLE_FIRESTORE_FIELD_PATH_H_
