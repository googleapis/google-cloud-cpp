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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_OBJECT_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_OBJECT_REQUEST_H

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// Common attributes for requests about objects.
template <typename Derived, typename... Parameters>
class GenericObjectRequest : public GenericRequest<Derived, Parameters...> {
 public:
  GenericObjectRequest() = default;

  explicit GenericObjectRequest(std::string bucket_name,
                                std::string object_name)
      : bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  Derived& set_bucket_name(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
    return *static_cast<Derived*>(this);
  }

  std::string const& object_name() const { return object_name_; }
  Derived& set_object_name(std::string object_name) {
    object_name_ = std::move(object_name);
    return *static_cast<Derived*>(this);
  }

 private:
  std::string bucket_name_;
  std::string object_name_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_OBJECT_REQUEST_H
