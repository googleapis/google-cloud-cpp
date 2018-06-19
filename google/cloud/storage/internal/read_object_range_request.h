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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_READ_OBJECT_RANGE_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_READ_OBJECT_RANGE_REQUEST_H_

#include "google/cloud/storage/internal/request_parameters.h"
#include "google/cloud/storage/well_known_parameters.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Request a range of object data.
 */
class ReadObjectRangeRequest
    : private RequestParameters<Generation, IfGenerationMatch,
                                IfGenerationNotMatch, IfMetaGenerationMatch,
                                IfMetaGenerationNotMatch, UserProject> {
 public:
  ReadObjectRangeRequest() = default;

  // TODO(#724) - consider using StrongType<> for arguments with similar types.
  explicit ReadObjectRangeRequest(std::string bucket_name,
                                  std::string object_name, std::int64_t begin,
                                  std::int64_t end)
      : bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)),
        begin_(begin),
        end_(end) {}

  // TODO(#724) - consider using StrongType<> for arguments with similar types.
  explicit ReadObjectRangeRequest(std::string bucket_name,
                                  std::string object_name)
      : bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)),
        begin_(0),
        end_(0) {}

  std::string const& bucket_name() const { return bucket_name_; }
  ReadObjectRangeRequest& set_bucket_name(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
    return *this;
  }

  std::string const& object_name() const { return object_name_; }
  ReadObjectRangeRequest& set_object_name(std::string object_name) {
    object_name_ = std::move(object_name);
    return *this;
  }

  std::int64_t begin() const { return begin_; }
  ReadObjectRangeRequest& set_begin(std::int64_t v) {
    begin_ = v;
    return *this;
  }

  std::int64_t end() const { return end_; }
  ReadObjectRangeRequest& set_end(std::int64_t v) {
    end_ = v;
    return *this;
  }

  template <typename Parameter>
  ReadObjectRangeRequest& set_parameter(Parameter&& p) {
    RequestParameters::set_parameter(std::forward<Parameter>(p));
    return *this;
  }

  template <typename... Parameters>
  ReadObjectRangeRequest& set_multiple_parameters(Parameters&&... p) {
    RequestParameters::set_multiple_parameters(std::forward<Parameters>(p)...);
    return *this;
  }

  using RequestParameters::AddParametersToHttpRequest;

 private:
  std::string bucket_name_;
  std::string object_name_;
  std::int64_t begin_;
  std::int64_t end_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_READ_OBJECT_RANGE_REQUEST_H_
