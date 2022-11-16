// Copyright 2022 Google LLC
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

#include "google/cloud/internal/error_metadata.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string Format(absl::string_view message, ErrorContext const& context) {
  if (context.empty()) return std::string{message};
  auto format = [](std::string* out, auto const& i) {
    absl::StrAppend(out, i.first, "=", i.second);
  };
  return absl::StrCat(message, ", ", absl::StrJoin(context, ", ", format));
}

std::string Format(absl::string_view message,
                   SavedErrorContext const& context) {
  if (context.empty()) return std::string{message};
  auto format = [](std::string* out, auto const& i) {
    absl::StrAppend(out, i.first, "=", i.second);
  };
  return absl::StrCat(message, ", ", absl::StrJoin(context, ", ", format));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
