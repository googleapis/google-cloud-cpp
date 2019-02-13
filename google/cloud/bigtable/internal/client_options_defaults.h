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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_OPTIONS_DEFAULTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_OPTIONS_DEFAULTS_H_

#include "google/cloud/bigtable/version.h"

// Make the default pool size 4 because that is consistent with what Go does.
#ifndef BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE
#define BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE 4
#endif  // BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE

#ifndef BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU
#define BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU 2
#endif  // BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU

#ifndef BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH
#define BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH (256 * 1024L * 1024L)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_OPTIONS_DEFAULTS_H_
