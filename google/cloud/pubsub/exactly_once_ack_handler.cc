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

#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

static_assert(!std::is_copy_assignable<ExactlyOnceAckHandler>::value,
              "AckHandler should not be CopyAssignable");
static_assert(!std::is_copy_constructible<ExactlyOnceAckHandler>::value,
              "AckHandler should not be CopyConstructible");
static_assert(std::is_move_assignable<ExactlyOnceAckHandler>::value,
              "AckHandler should be MoveAssignable");
static_assert(std::is_move_constructible<ExactlyOnceAckHandler>::value,
              "AckHandler should be MoveConstructible");

ExactlyOnceAckHandler::~ExactlyOnceAckHandler() {
  if (impl_) impl_->nack();
}

ExactlyOnceAckHandler::Impl::~Impl() = default;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
