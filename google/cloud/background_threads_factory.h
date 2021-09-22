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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_FACTORY_H

#include "google/cloud/background_threads.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

/// A function to generate `BackgroundThreads`
using BackgroundThreadsFactory =
    std::function<std::unique_ptr<BackgroundThreads>()>;

/// Create a `BackgroundThreadsFactory` that uses @p cq for all background work.
BackgroundThreadsFactory CustomBackgroundThreads(CompletionQueue const& cq);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BACKGROUND_THREADS_FACTORY_H
