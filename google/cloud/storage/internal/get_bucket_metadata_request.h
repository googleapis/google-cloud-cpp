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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GET_BUCKET_METADATA_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GET_BUCKET_METADATA_REQUEST_H_

#include "google/cloud/storage/internal/request_parameters.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Request the metadata for a bucket.
 */
class GetBucketMetadataRequest
    : private RequestParameters<IfMetaGenerationMatch, IfMetaGenerationNotMatch,
                                Projection, UserProject> {
 public:
  GetBucketMetadataRequest() = default;
  explicit GetBucketMetadataRequest(std::string bucket_name)
      : bucket_name_(std::move(bucket_name)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  GetBucketMetadataRequest& set_bucket_name(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
    return *this;
  }

  /**
   * Set a single optional parameter.
   *
   * @tparam Parameter the type of the parameter.
   * @param p the parameter value.
   * @return a reference to this object for chaining.
   */
  template <typename Parameter>
  GetBucketMetadataRequest& set_parameter(Parameter&& p) {
    RequestParameters::set_parameter(std::forward<Parameter>(p));
    return *this;
  }

  /**
   * Change one or more parameters for the request.
   *
   * This is a shorthand to replace:
   *
   * @code
   * request.set_parameter(m1).set_parameter(m2).set_parameter(m3)
   * @endcode
   *
   * with:
   *
   * @code
   * request.set_multiple_parameters(m1, m2, m3)
   * @endcode

   * @tparam Parameters the type of the parameters
   * @param p the parameter values
   * @return The object after all the parameters have been changed.
   */
  template <typename... Parameters>
  GetBucketMetadataRequest& set_multiple_parameters(Parameters&&... p) {
    RequestParameters::set_multiple_parameters(std::forward<Parameters>(p)...);
    return *this;
  }

  using RequestParameters::AddParametersToHttpRequest;

  /// Dump parameter values to a std::ostream
  using RequestParameters::DumpParameters;

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, GetBucketMetadataRequest const& r);
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GET_BUCKET_METADATA_REQUEST_H_
