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

#include "google/cloud/storage/internal/generic_object_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/well_known_parameters.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Request a range of object data.
 */
class ReadObjectRangeRequest
    : public GenericObjectRequest<ReadObjectRangeRequest, Generation,
                                  IfGenerationMatch, IfGenerationNotMatch,
                                  IfMetaGenerationMatch,
                                  IfMetaGenerationNotMatch, UserProject> {
 public:
  ReadObjectRangeRequest() : GenericObjectRequest(), begin_(0), end_(0) {}

  // TODO(#724) - consider using StrongType<> for arguments with similar types.
  explicit ReadObjectRangeRequest(std::string bucket_name,
                                  std::string object_name, std::int64_t begin,
                                  std::int64_t end)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        begin_(begin),
        end_(end) {}

  // TODO(#724) - consider using StrongType<> for arguments with similar types.
  explicit ReadObjectRangeRequest(std::string bucket_name,
                                  std::string object_name)
      : GenericObjectRequest(std::move(bucket_name), std::move(object_name)),
        begin_(0),
        end_(0) {}

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

 private:
  std::int64_t begin_;
  std::int64_t end_;
};

std::ostream& operator<<(std::ostream& os, ReadObjectRangeRequest const& r);

struct ReadObjectRangeResponse {
  std::string contents;
  std::int64_t first_byte;
  std::int64_t last_byte;
  std::int64_t object_size;

  static ReadObjectRangeResponse FromHttpResponse(HttpResponse&& response);
};

std::ostream& operator<<(std::ostream& os, ReadObjectRangeResponse const& r);
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_READ_OBJECT_RANGE_REQUEST_H_
