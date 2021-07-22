// Copyright 2021 Google LLC
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

#include "google/cloud/storage/object_read_stream.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
std::unique_ptr<internal::ObjectReadStreambuf> MakeErrorStreambuf() {
  return absl::make_unique<internal::ObjectReadStreambuf>(
      internal::ReadObjectRangeRequest("", ""),
      Status(StatusCode::kUnimplemented, "null stream"));
}
}  // namespace

static_assert(std::is_move_assignable<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move assignable.");
static_assert(std::is_move_constructible<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move constructible.");

ObjectReadStream::ObjectReadStream() : ObjectReadStream(MakeErrorStreambuf()) {}

ObjectReadStream::ObjectReadStream(ObjectReadStream&& rhs) noexcept
    : std::basic_istream<char>(std::move(rhs)),
      // The spec guarantees the base class move constructor only changes a few
      // member variables in `std::basic_istream<>`, and there is no spooky
      // action through virtual functions because there are no virtual
      // functions.  A good summary of the specification is "it calls the
      // default constructor and then calls std::basic_ios<>::move":
      //   https://en.cppreference.com/w/cpp/io/basic_ios/move
      // In fact, as that page indicates, the base classes are designed such
      // that derived classes can define their own move constructor and move
      // assignment.
      buf_(std::move(rhs.buf_)) {
  rhs.buf_ = MakeErrorStreambuf();
  rhs.set_rdbuf(rhs.buf_.get());
  set_rdbuf(buf_.get());
}

ObjectReadStream::~ObjectReadStream() {
  if (!IsOpen()) {
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
  if (!IsOpen()) {
    return;
  }
  buf_->Close();
  if (!status().ok()) {
    setstate(std::ios_base::badbit);
  }
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
