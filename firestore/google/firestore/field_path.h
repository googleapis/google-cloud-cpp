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
  FieldPath(const std::vector<std::string>& parts);

  // Construct FieldPath from a field path string
  static const FieldPath from_string(const std::string& string);

  // The regex for a simple field name
  static std::regex simple_field_name;

  // Construct a new FieldPath by appending a field path string
  const FieldPath append(const std::string& path) const;

  // Construct a new FieldPath by appending a field path string
  const FieldPath append(const FieldPath& field_path) const;

  // Convert the FieldPath into a unique representation for the server
  const std::string to_api_repr() const;

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
  static std::vector<std::string> split(const std::string& string);

  // The components of this FieldPath
  std::vector<std::string> parts;
};
}  // namespace firestore
#endif
