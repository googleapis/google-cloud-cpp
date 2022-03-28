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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULTS_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <google/cloud/pubsublite/v1/common.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::size_t DefaultThreadCount();

Options DefaultCommonOptions(Options opts);

Options DefaultPublisherOptions(Options opts);

Options DefaultPublisherOptionsOnly(Options opts);

Options DefaultSubscriberOptions(Options opts);

Options DefaultSubscriberOptionsOnly(Options opts);

StatusOr<google::cloud::pubsublite::v1::SequencedMessage>
DefaultPublishMessageTransformer(const google::cloud::pubsub::Message&);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_DEFAULTS_H
