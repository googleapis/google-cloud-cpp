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

#include "google/cloud/pubsub/ack_handler.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

static_assert(!std::is_copy_assignable<AckHandler>::value,
              "AckHandler should not be CopyAssignable");
static_assert(!std::is_copy_constructible<AckHandler>::value,
              "AckHandler should not be CopyConstructible");
static_assert(std::is_move_assignable<AckHandler>::value,
              "AckHandler should be MoveAssignable");
static_assert(std::is_move_constructible<AckHandler>::value,
              "AckHandler should be MoveConstructible");

AckHandler::~AckHandler() {
  if (impl_) impl_->nack();
}

AckHandler::Impl::~Impl() = default;

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
