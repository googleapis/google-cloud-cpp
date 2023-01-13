// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_BUILDERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_BUILDERS_H

#include "google/cloud/storage/internal/xml_node.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

constexpr unsigned int kMaxPartNumber = 10000;

/**
 * A builder for an XML request for `Completing multipart upload` API
 * described at
 * https://cloud.google.com/storage/docs/xml-api/post-object-complete.
 *
 * @par Thread-safety
 * This class is meant to be shared among multiple threads.
 * Each thread can add the part information to the builder.
 */
class CompleteMultipartUploadXmlBuilder {
 public:
  static std::shared_ptr<CompleteMultipartUploadXmlBuilder> Create() {
    return std::shared_ptr<CompleteMultipartUploadXmlBuilder> {
        new CompleteMultipartUploadXmlBuilder()};
  }

  ~CompleteMultipartUploadXmlBuilder() = default;

  /// Add a part (pair of int and string).
  Status AddPart(unsigned int part_number, std::string etag);
  /// Build an Xml tree and return it.
  std::shared_ptr<XmlNode const> Build();

 private:
  CompleteMultipartUploadXmlBuilder() = default;

  std::mutex part_map_mu_;
  // The part number needs to be sorted in the final xml.
  std::map<unsigned int, std::string> part_map_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_XML_BUILDERS_H
