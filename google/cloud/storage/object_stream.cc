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

#include "google/cloud/storage/object_stream.h"
#include "google/cloud/log.h"
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_move_assignable<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move assignable.");
static_assert(std::is_move_constructible<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move constructible.");

ObjectReadStream::~ObjectReadStream() {
  if (not IsOpen()) {
    return;
  }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    Close();
  } catch (std::exception const& ex) {
    GCP_LOG(INFO) << "Ignored exception while trying to close stream: "
                  << ex.what();
  } catch (...) {
    GCP_LOG(INFO) << "Ignored unknown exception while trying to close stream";
  }
#else
  Close();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

void ObjectReadStream::Close() {
  if (not IsOpen()) {
    return;
  }
  buf_->Close();
  if (not status().ok()) {
    setstate(std::ios_base::badbit);
  }
}

ObjectWriteStream::~ObjectWriteStream() {
  if (not IsOpen()) {
    return;
  }
  // Disable exceptions, even if the application had enabled exceptions the
  // destructor is supposed to mask them.
  exceptions(std::ios_base::goodbit);
  Close();
}

void ObjectWriteStream::Close() {
  if (not IsOpen()) {
    return;
  }
  StatusOr<internal::HttpResponse> response = buf_->Close();
  if (not response.ok()) {
    metadata_ = StatusOr<ObjectMetadata>(std::move(response).status());
    setstate(std::ios_base::badbit);
    return;
  }
  headers_ = std::move(response->headers);
  payload_ = std::move(response->payload);
  if (response->status_code >= 300) {
    metadata_ =
        StatusOr<ObjectMetadata>(Status(response->status_code, payload_));
    setstate(std::ios_base::badbit);
    return;
  }
  if (payload_.empty()) {
    // With the XML transport the response includes an empty payload, in that
    // case it cannot be parsed.
    metadata_ = ObjectMetadata{};
  } else {
    metadata_ = ObjectMetadata::ParseFromString(payload_);
    if (not metadata_.ok()) {
      setstate(std::ios_base::badbit);
      return;
    }
  }

  if (not buf_->ValidateHash(*metadata_)) {
    setstate(std::ios_base::badbit);
  }
}

void ObjectWriteStream::Suspend() && { buf_.reset(); }

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
