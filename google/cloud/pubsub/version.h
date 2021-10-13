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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H

#include "google/cloud/pubsub/version_info.h"
#include "google/cloud/version.h"

// This preprocessor symbol is deprecated and should never be used anywhere. It
// exists solely for backward compatibility to avoid breaking anyone who may
// have been using it.
#define GOOGLE_CLOUD_CPP_PUBSUB_NS GOOGLE_CLOUD_CPP_NS

namespace google {
/**
 * The namespace Google Cloud Platform C++ client libraries.
 */
namespace cloud {
/**
 * Contains all the Cloud Pubsub C++ client types and functions.
 */
namespace pubsub {
/**
 * Versioned inline namespace that users should generally avoid spelling.
 *
 * Applications may need to link multiple versions of the Cloud pubsub C++
 * client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `pubsub::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `pubsub::v1::Foo`.
 */
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_VERSION_H
