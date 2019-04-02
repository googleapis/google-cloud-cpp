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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_H_

#include "google/cloud/storage/version.h"
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define a condition for a policy document.
 */
class PolicyDocumentCondition {
 public:
  PolicyDocumentCondition() = default;
  PolicyDocumentCondition(std::vector<std::string> elements)
      : elements_(elements) {}

  std::vector<std::string> const& elements() const { return elements_; }

  //@{
  /**
   * @name Creates different types of PolicyDocumentCondition matchers.
   */

  /// Creates an exact match condition, in the list form syntax.
  static std::vector<std::string> ExactMatch(std::string const& field,
                                             std::string const& value) {
    std::vector<std::string> result;
    result.emplace_back("eq");
    result.emplace_back(std::string("$") + field);
    result.emplace_back(value);
    return result;
  }

  /// Creates an exact match condition, but in object form syntax.
  static std::vector<std::string> ExactMatchObject(std::string const& field,
                                                   std::string const& value) {
    std::vector<std::string> result;
    result.emplace_back(field);
    result.emplace_back(value);
    return result;
  }

  static std::vector<std::string> StartsWith(std::string const& field,
                                             std::string const& value) {
    std::vector<std::string> result;
    result.emplace_back("starts-with");
    result.emplace_back(std::string("$") + field);
    result.emplace_back(value);
    return result;
  }

  static std::vector<std::string> ContentLengthRange(std::int32_t min_range,
                                                     std::int32_t max_range) {
    std::vector<std::string> result;
    result.emplace_back("content-length-range");
    result.emplace_back(std::to_string(min_range));
    result.emplace_back(std::to_string(max_range));
    return result;
  }
  //@}

 private:
  std::vector<std::string> elements_;
};

inline bool operator==(PolicyDocumentCondition const& lhs,
                       PolicyDocumentCondition const& rhs) {
  return lhs.elements() == rhs.elements();
}

inline bool operator<(PolicyDocumentCondition const& lhs,
                      PolicyDocumentCondition const& rhs) {
  return lhs.elements() < rhs.elements();
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

/**
 * Define a policy document.
 *
 * Policy documents allow HTML forms to restrict uploads based on certain
 * conditions. If the policy document is expired or the conditions are not
 * satisified, POST'ing the form will not succeed.
 *
 * @see https://cloud.google.com/storage/docs/xml-api/post-object#policydocument
 * for general information on policy documents in Google Cloud Storage.
 */
struct PolicyDocument {
  std::chrono::system_clock::time_point expiration;
  std::vector<PolicyDocumentCondition> conditions;
};

std::ostream& operator<<(std::ostream& os, PolicyDocument const& rhs);

/**
 * Define a policy document result.
 *
 * `access_id` is the the Cloud Storage email form of the client ID. `policy`
 * is the base64 encoded form of the plain-text policy document and `signature`
 * is the signed policy document.
 */
struct PolicyDocumentResult {
  std::string access_id;
  std::chrono::system_clock::time_point expiration;
  std::string policy;
  std::string signature;
};

std::ostream& operator<<(std::ostream& os, PolicyDocumentResult const& rhs);
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_POLICY_DOCUMENT_H_
