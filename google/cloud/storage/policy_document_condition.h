// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_CONDITION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_CONDITION_H_

#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {}  // namespace internal

struct PolicyDocumentEntry {
  std::vector<std::string> elements;
};

inline bool operator==(PolicyDocumentEntry const& lhs,
                       PolicyDocumentEntry const& rhs) {
  return lhs.elements == rhs.elements;
}

inline bool operator<(PolicyDocumentEntry const& lhs,
                      PolicyDocumentEntry const& rhs) {
  return lhs.elements < rhs.elements;
}

inline bool operator!=(PolicyDocumentEntry const& lhs,
                       PolicyDocumentEntry const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(PolicyDocumentEntry const& lhs,
                      PolicyDocumentEntry const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(PolicyDocumentEntry const& lhs,
                       PolicyDocumentEntry const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(PolicyDocumentEntry const& lhs,
                       PolicyDocumentEntry const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentEntry const& rhs);

/**
 * @see https://cloud.google.com/storage/docs/xml-api/post-object#policydocument
 */
struct PolicyDocumentCondition {
  explicit PolicyDocumentCondition(PolicyDocumentEntry entry)
      : entry_(std::move(entry)) {}

  PolicyDocumentEntry const& entry() const { return entry_; }

  //@{
  /**
   * @name Creates different types of PolicyDocumentCondition matchers.
   */
  static PolicyDocumentEntry ExactMatch(std::string const& field,
                                        std::string const& value) {
    PolicyDocumentEntry result;
    result.elements.emplace_back("eq");
    result.elements.emplace_back(std::string("$") + field);
    result.elements.emplace_back(value);
    return result;
  }

  static PolicyDocumentEntry StartsWith(std::string const& field,
                                        std::string const& value) {
    PolicyDocumentEntry result;
    result.elements.emplace_back("starts-with");
    result.elements.emplace_back(std::string("$") + field);
    result.elements.emplace_back(value);
    return result;
  }

  static PolicyDocumentEntry ContentLengthRange(std::int32_t min_range,
                                                std::int32_t max_range) {
    PolicyDocumentEntry result;
    result.elements.emplace_back("content-length-range");
    result.elements.emplace_back(std::to_string(min_range));
    result.elements.emplace_back(std::to_string(max_range));
    return result;
  }
  //@}

 private:
  PolicyDocumentEntry entry_;
};

inline bool operator==(PolicyDocumentCondition const& lhs,
                       PolicyDocumentCondition const& rhs) {
  return lhs.entry() == rhs.entry();
}

inline bool operator<(PolicyDocumentCondition const& lhs,
                      PolicyDocumentCondition const& rhs) {
  return lhs.entry().elements < rhs.entry().elements;
}

inline bool operator!=(PolicyDocumentCondition const& lhs,
                       PolicyDocumentCondition const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(PolicyDocumentCondition const& lhs,
                      PolicyDocumentCondition const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(PolicyDocumentCondition const& lhs,
                       PolicyDocumentCondition const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(PolicyDocumentCondition const& lhs,
                       PolicyDocumentCondition const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentCondition const& rhs);
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_CONDITION_H_
