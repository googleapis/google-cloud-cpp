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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ABSL_STR_JOIN_QUIET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ABSL_STR_JOIN_QUIET_H

// TODO(#4501) - fix by doing #include <absl/...>
#include "google/cloud/internal/diagnostics_push.inc"
#if _MSC_VER
#pragma warning(disable : 4244)
#endif  // _MSC_VER
#include "absl/strings/str_join.h"
// TODO(#4501) - end
#include "google/cloud/internal/diagnostics_push.inc"

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ABSL_STR_JOIN_QUIET_H
