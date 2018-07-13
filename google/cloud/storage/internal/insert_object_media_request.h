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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_INSERT_OBJECT_MEDIA_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_INSERT_OBJECT_MEDIA_REQUEST_H_

#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/well_known_parameters.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Insert an object with a simple std::string for its media.
 */
class InsertObjectMediaRequest
    : public GenericObjectRequest<
          InsertObjectMediaRequest, ContentEncoding, IfGenerationMatch,
          IfGenerationNotMatch, IfMetaGenerationMatch, IfMetaGenerationNotMatch,
          KmsKeyName, PredefinedAcl, Projection, UserProject> {
 public:
  InsertObjectMediaRequest() : GenericObjectRequest(), contents_() {}

  explicit InsertObjectMediaRequest(std::string bucket_name,
                                    std::string object_name,
                                    std::string contents)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        contents_(std::move(contents)) {}

  std::string const& contents() const { return contents_; }
  InsertObjectMediaRequest& set_contents(std::string contents) {
    contents_ = std::move(contents);
    return *this;
  }

 private:
  std::string contents_;
};

std::ostream& operator<<(std::ostream& os, InsertObjectMediaRequest const& r);

/**
 * Insert an object with streaming media.
 */
class InsertObjectStreamingRequest
    : public GenericObjectRequest<
          InsertObjectStreamingRequest, ContentEncoding, IfGenerationMatch,
          IfGenerationNotMatch, IfMetaGenerationMatch, IfMetaGenerationNotMatch,
          KmsKeyName, PredefinedAcl, Projection, UserProject> {
 public:
  using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os,
                         InsertObjectStreamingRequest const& r);
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_INSERT_OBJECT_MEDIA_REQUEST_H_
