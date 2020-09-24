// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_REMOVE_STALE_BUCKETS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_REMOVE_STALE_BUCKETS_H

#include "google/cloud/storage/client.h"
#include "google/cloud/status.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/// Remove a bucket, including any objects in it
Status RemoveBucketAndContents(google::cloud::storage::Client client,
                               std::string const& bucket_name);

/**
 * Remove stale buckets created for examples.
 *
 * The examples and integration tests create buckets in the production
 * environment. While these programs are supposed to clean after themselves,
 * they might crash or otherwise fail to delete any buckets they create. These
 * buckets can accumulate and cause future tests to fail (see #4905). To prevent
 * these problems we delete any bucket that match the pattern of these randomly
 * created buckets, as long as the bucket was created more than 48 hours ago.
 *
 * @param client used to make calls to GCS.
 * @param prefix only delete buckets that start with this string followed by a
 *   date (in `YYYY-mm-dd` format), and then an underscore character (`_`).
 * @param created_time_limit only delete buckets created before this timestamp.
 */
Status RemoveStaleBuckets(
    google::cloud::storage::Client client, std::string const& prefix,
    std::chrono::system_clock::time_point created_time_limit);

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_REMOVE_STALE_BUCKETS_H
