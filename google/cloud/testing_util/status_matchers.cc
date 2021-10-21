// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util_internal {

using ::testing::Matcher;
using ::testing::MatchResultListener;
using ::testing::StringMatchResultListener;

namespace {

template <typename T>
void Explain(char const* field, bool matched, Matcher<T> const& matcher,
             StringMatchResultListener* listener, std::ostream* os) {
  std::string const explanation = listener->str();
  listener->Clear();
  *os << field;
  if (!explanation.empty()) {
    *os << " " << explanation;
  } else {
    *os << " that ";
    if (matched) {
      matcher.DescribeTo(os);
    } else {
      matcher.DescribeNegationTo(os);
    }
  }
}

}  // namespace

bool StatusIsMatcher::MatchAndExplain(Status const& status,
                                      MatchResultListener* listener) const {
  if (!listener->IsInterested()) {
    return code_matcher_.Matches(status.code()) &&
           message_matcher_.Matches(status.message());
  }
  auto* os = listener->stream();
  StringMatchResultListener inner_listener;
  auto const code_matched =
      code_matcher_.MatchAndExplain(status.code(), &inner_listener);
  *os << "with a ";
  Explain("code", code_matched, code_matcher_, &inner_listener, os);
  auto const message_matched =
      message_matcher_.MatchAndExplain(status.message(), &inner_listener);
  *os << ", " << (code_matched != message_matched ? "but" : "and") << " a ";
  Explain("message", message_matched, message_matcher_, &inner_listener, os);
  return code_matched && message_matched;
}

void StatusIsMatcher::DescribeTo(std::ostream* os) const {
  *os << "code ";
  code_matcher_.DescribeTo(os);
  *os << " and message ";
  message_matcher_.DescribeTo(os);
}

void StatusIsMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "code ";
  code_matcher_.DescribeNegationTo(os);
  *os << " or message ";
  message_matcher_.DescribeNegationTo(os);
}

}  // namespace testing_util_internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
