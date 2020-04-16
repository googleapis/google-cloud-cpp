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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_STREAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_STREAM_H

#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
class ReadStream;

namespace internal {
ReadStream MakeReadStream(std::string stream_name);
}  // namespace internal

class ReadStream {
 public:
  ReadStream(ReadStream const&) = default;
  ReadStream(ReadStream&&) = default;
  ReadStream& operator=(ReadStream const&) = default;
  ReadStream& operator=(ReadStream&&) = default;

  std::string const& stream_name() const { return stream_name_; }

  friend bool operator==(ReadStream const& lhs, ReadStream const& rhs) {
    return lhs.stream_name_ == rhs.stream_name_;
  }
  friend bool operator!=(ReadStream const& lhs, ReadStream const& rhs) {
    return !(lhs == rhs);
  }

 private:
  friend ReadStream internal::MakeReadStream(std::string stream_name);
  explicit ReadStream(std::string stream_name)
      : stream_name_(std::move(stream_name)) {}

  std::string stream_name_;
};

// Serializes an instance of `ReadStream` for transmission to another process.
std::string SerializeReadStream(ReadStream const& /*read_stream*/);

// Deserializes the provided string to a `ReadStream`, if able.
StatusOr<ReadStream> DeserializeReadStream(
    std::string const& /*serialized_read_stream*/);

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_STREAM_H
