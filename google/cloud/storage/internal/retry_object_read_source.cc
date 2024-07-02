// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/internal/make_status.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::google::cloud::internal::OptionsSpan;

std::uint64_t InitialOffset(OffsetDirection const& offset_direction,
                            ReadObjectRangeRequest const& request) {
  if (offset_direction == kFromEnd) {
    return request.GetOption<ReadLast>().value();
  }
  return request.StartingByte();
}

RetryObjectReadSource::RetryObjectReadSource(
    ReadSourceFactory factory,
    google::cloud::internal::ImmutableOptions options,
    ReadObjectRangeRequest request, std::unique_ptr<ObjectReadSource> child,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::function<void(std::chrono::milliseconds)> backoff)
    : factory_(std::move(factory)),
      options_(std::move(options)),
      request_(std::move(request)),
      child_(std::move(child)),
      retry_policy_prototype_(std::move(retry_policy)),
      backoff_policy_prototype_(std::move(backoff_policy)),
      offset_direction_(request_.HasOption<ReadLast>() ? kFromEnd
                                                       : kFromBeginning),
      current_offset_(InitialOffset(offset_direction_, request_)),
      backoff_(std::move(backoff)) {}

RetryObjectReadSource::RetryObjectReadSource(
    ReadSourceFactory factory,
    google::cloud::internal::ImmutableOptions options,
    ReadObjectRangeRequest request, std::unique_ptr<ObjectReadSource> child,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy)
    : RetryObjectReadSource(
          std::move(factory), std::move(options), std::move(request),
          std::move(child), std::move(retry_policy), std::move(backoff_policy),
          [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); }) {
}

StatusOr<ReadSourceResult> RetryObjectReadSource::Read(char* buf,
                                                       std::size_t n) {
  if (!child_) {
    return google::cloud::internal::FailedPreconditionError(
        "Stream is not open", GCP_ERROR_INFO());
  }

  // Read some data, if successful return immediately, saving some allocations.
  auto result = child_->Read(buf, n);
  if (HandleResult(result)) return result;
  bool has_emulator_instructions = false;
  std::string instructions;
  if (request_.HasOption<CustomHeader>()) {
    auto name = request_.GetOption<CustomHeader>().custom_header_name();
    has_emulator_instructions = (name == "x-goog-emulator-instructions");
    instructions = request_.GetOption<CustomHeader>().value();
  }

  // Start a new retry loop to get the data.
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto retry_policy = retry_policy_prototype_->clone();
  int counter = 0;
  while (!result && retry_policy->OnFailure(result.status())) {
    // A Read() request failed, most likely that means the connection failed or
    // stalled. The current child might no longer be usable, so we will try to
    // create a new one and replace it. Should that fail, the retry policy would
    // already be exhausted, so we should fail this operation too.
    child_.reset();

    // The first attempt does not get to backoff.  The previous download was
    // working fine, so whatever caused the download to stop may not be an
    // overload condition.
    if (++counter != 1) {
      backoff_(backoff_policy->OnCompletion());
    }
    if (has_emulator_instructions) {
      request_.set_multiple_options(
          CustomHeader("x-goog-emulator-instructions",
                       instructions + "/retry-" + std::to_string(counter)));
    }

    if (offset_direction_ == kFromEnd) {
      request_.set_option(ReadLast(current_offset_));
    } else {
      request_.set_option(ReadFromOffset(current_offset_));
    }
    if (generation_) {
      request_.set_option(Generation(*generation_));
    }
    auto status = MakeChild(*retry_policy, *backoff_policy);
    if (!status.ok()) {
      result = status;
      continue;
    }
    result = child_->Read(buf, n);
  }
  if (HandleResult(result)) return result;
  // We have exhausted the retry policy, return the error.
  auto status = std::move(result).status();
  std::ostringstream os;
  if (internal::StatusTraits::IsPermanentFailure(status)) {
    os << "Permanent error in Read(): " << status.message();
  } else {
    os << "Retry policy exhausted in Read(): " << status.message();
  }
  return Status(status.code(), std::move(os).str(), status.error_info());
}

bool RetryObjectReadSource::HandleResult(StatusOr<ReadSourceResult> const& r) {
  if (!r) return false;
  if (r->generation) generation_ = r->generation;
  if (r->transformation.value_or("") == "gunzipped") is_gunzipped_ = true;
  // Since decompressive transcoding does not respect `ReadLast()` we need
  // to ensure the offset is incremented, so the discard loop works.
  if (is_gunzipped_) offset_direction_ = kFromBeginning;
  if (offset_direction_ == kFromEnd) {
    current_offset_ -= r->bytes_received;
  } else {
    current_offset_ += r->bytes_received;
  }
  return true;
}

Status RetryObjectReadSource::MakeChild(RetryPolicy& retry_policy,
                                        BackoffPolicy& backoff_policy) {
  auto on_success = [this](std::unique_ptr<ObjectReadSource> child) {
    child_ = std::move(child);
    return Status{};
  };

  OptionsSpan const span(options_);
  auto child = factory_(request_, retry_policy, backoff_policy);
  if (!child) return std::move(child).status();
  if (!is_gunzipped_) return on_success(*std::move(child));

  // Downloads under decompressive transcoding do not respect the Read-Range
  // header. Restarting the download effectively restarts the read from the
  // first byte.
  child = ReadDiscard(*std::move(child), current_offset_);
  if (child) return on_success(*std::move(child));
  return std::move(child).status();
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryObjectReadSource::ReadDiscard(
    std::unique_ptr<ObjectReadSource> child, std::int64_t count) const {
  // Discard data until we are at the same offset as before.
  std::vector<char> buffer(128 * 1024);
  while (count > 0) {
    auto const read_size =
        (std::min)(static_cast<std::int64_t>(buffer.size()), count);
    auto result =
        child->Read(buffer.data(), static_cast<std::size_t>(read_size));
    if (!result) return std::move(result).status();
    count -= result->bytes_received;
    if (result->response.status_code != HttpStatusCode::kContinue &&
        count != 0) {
      return google::cloud::internal::InternalError(
          "could not read back to previous offset (" +
              std::to_string(current_offset_) + ")",
          GCP_ERROR_INFO());
    }
  }
  return child;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
