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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SERVICE_ACCOUNT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SERVICE_ACCOUNT_H_

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents the metadata for a Google Cloud Storage service account.
 */
class ServiceAccount {
 public:
  ServiceAccount() = default;

  static StatusOr<ServiceAccount> ParseFromJson(internal::nl::json const& json);
  static StatusOr<ServiceAccount> ParseFromString(std::string const& payload);

  std::string const& email_address() const { return email_address_; }
  std::string const& kind() const { return kind_; }

 private:
  std::string email_address_;
  std::string kind_;
};

inline bool operator==(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::tie(lhs.email_address(), lhs.kind()) ==
         std::tie(rhs.email_address(), rhs.kind());
}

inline bool operator<(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::tie(lhs.email_address(), lhs.kind()) <
         std::tie(rhs.email_address(), rhs.kind());
}

inline bool operator!=(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(ServiceAccount const& lhs, ServiceAccount const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, ServiceAccount const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_SERVICE_ACCOUNT_H_
