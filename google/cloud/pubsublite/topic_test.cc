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

#include "google/cloud/pubsublite/topic.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>

using google::cloud::pubsublite::Topic;

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(Topic, BasicTopic) {
  std::string project = "project";
  std::string location = "location";
  std::string topic_name = "topic_name";

  Topic topic{project, location, topic_name};
  EXPECT_EQ(project, topic.project());
  EXPECT_EQ(location, topic.location());
  EXPECT_EQ(topic_name, topic.topic_name());
  EXPECT_EQ(topic.FullName(),
            "projects/project/locations/location/topics/topic_name");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
