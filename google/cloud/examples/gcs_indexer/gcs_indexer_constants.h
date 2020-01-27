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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EXAMPLES_GCS_INDEXER_GCS_INDEXER_CONSTANTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EXAMPLES_GCS_INDEXER_GCS_INDEXER_CONSTANTS_H_

#include <string>

inline constexpr std::string_view table_name = "gcs_objects";
inline constexpr char const* column_names[] = {
    "bucket",
    "object",
    "generation",
    "meta_generation",
    "is_archived",
    "size",
    "content_type",
    "time_created",
    "updated",
    "storage_class",
    "time_storage_class_updated",
    "md5_hash",
    "crc32c",
    "event_timestamp",
};

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_EXAMPLES_GCS_INDEXER_GCS_INDEXER_CONSTANTS_H_
