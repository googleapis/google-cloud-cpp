// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ATTRIBUTES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ATTRIBUTES_H

#include "google/cloud/internal/port_platform.h"

/**
 * Marks a class, struct, enum, function, member function, or variable as
 * deprecated.
 *
 * @note If you need to temporarily disable the deprecation warnings in your
 *     application, consider adding a pre-processor flag to define
 *     `GOOGLE_CLOUD_CPP_DISABLE_DEPRECATION_WARNINGS`.
 *
 * @note Any unit test, example, or integration test that uses deprecated
 *     symbols would break in the `google-cloud-cpp` CI builds unless the
 *     warnings are disabled using `internal/disable_deprecation_warnings.inc`.
 */
#if defined(GOOGLE_CLOUD_CPP_DEPRECATED)
#error "Applications should not define GOOGLE_CLOUD_CPP_DEPRECATED"
#elif GOOGLE_CLOUD_CPP_DISABLE_DEPRECATION_WARNINGS
#define GOOGLE_CLOUD_CPP_DEPRECATED(message)
#else
#define GOOGLE_CLOUD_CPP_DEPRECATED(message) [[deprecated(message)]]
#endif

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ATTRIBUTES_H
